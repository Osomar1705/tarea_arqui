#include <cstdio>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "isa.hpp"
#include "simulator.hpp"

static const uint64_t DEFAULT_MAX_STEPS = 10'000'000;

struct Repl {
    Simulator sim;
    std::set<uint32_t> breakpoints;
    bool trace = false;
    bool loaded = false;
    bool interactive = true;
    std::string current_file;

    static bool parse_u32(const std::string& s, uint32_t& v) {
        if (s.empty())
            return false;
        try {
            size_t idx = 0;
            long long n = std::stoll(s, &idx, 0);
            if (idx != s.size() || n < INT32_MIN || n > UINT32_MAX)
                return false;
            v = static_cast<uint32_t>(n);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool parse_addr(const std::string& s, uint32_t& v) {
        if (s == "pc") {
            v = sim.get_pc();
            return true;
        }
        int r = isa::parse_register(s);
        if (r >= 0) {
            v = sim.get_reg(r);
            return true;
        }
        return parse_u32(s, v);
    }

    static std::string hex(uint32_t v) {
        char buf[16];
        snprintf(buf, sizeof(buf), "0x%08X", v);
        return buf;
    }

    std::string disasm_at(uint32_t addr) {
        uint32_t instr = sim.memory().read32(addr);
        char buf[200];
        snprintf(buf, sizeof(buf), "%s:  %08X   %s", hex(addr).c_str(), instr,
                 isa::disassemble(instr, addr).c_str());
        return buf;
    }

    void print_reg(int i) {
        uint32_t v = sim.get_reg(i);
        char name[16];
        snprintf(name, sizeof(name), "x%d (%s)", i, isa::ABI_NAMES[i]);
        printf("%-10s = %s  (%d)\n", name, hex(v).c_str(), static_cast<int32_t>(v));
    }

    bool require_program() {
        if (!loaded)
            std::cout << "No hay programa cargado. Usa: load <archivo>\n";
        return loaded;
    }

    bool do_step(bool verbose) {
        if (sim.is_exited()) {
            std::cout << "El programa ya terminó. Usa reset para volver a empezar.\n";
            return false;
        }
        uint32_t pc = sim.get_pc();
        std::string text = disasm_at(pc);
        StepResult r = sim.step();
        if (verbose || trace)
            std::cout << "Ejecutando " << text << "\n";
        switch (r) {
            case StepResult::Ok:
                return true;
            case StepResult::Exited:
                std::cout << "\n[El programa terminó con código " << sim.get_exit_code() << "]\n";
                return false;
            case StepResult::Breakpoint:
                std::cout << "[ebreak en " << hex(pc) << "]\n";
                return false;
            case StepResult::Error:
                std::cout << "[Error] " << sim.get_error() << "\n";
                return false;
        }
        return false;
    }

    void cmd_load(const std::string& file) {
        std::string err;
        if (!sim.load_from_file(file, err)) {
            std::cout << "Error al cargar: " << err << "\n";
            return;
        }
        loaded = true;
        current_file = file;
        std::cout << "\"" << file << "\" cargado a memoria (entrada en " << hex(sim.get_pc())
                  << ").\n";
    }

    void cmd_step(const std::vector<std::string>& args) {
        if (!require_program())
            return;
        uint32_t n = 1;
        if (args.size() > 1 && !parse_u32(args[1], n)) {
            std::cout << "Uso: step [n]\n";
            return;
        }
        for (uint32_t i = 0; i < n; i++) {
            if (!do_step(true))
                return;
            if (i + 1 < n && breakpoints.count(sim.get_pc())) {
                std::cout << "[Breakpoint en " << hex(sim.get_pc()) << "]\n";
                return;
            }
        }
    }

    void cmd_run(const std::vector<std::string>& args) {
        if (!require_program())
            return;
        uint32_t max = DEFAULT_MAX_STEPS;
        if (args.size() > 1 && !parse_u32(args[1], max)) {
            std::cout << "Uso: run [max_instrucciones]\n";
            return;
        }
        uint64_t start = sim.get_instret();
        for (uint64_t i = 0; i < max; i++) {
            if (!do_step(false))
                break;
            if (sim.in_self_loop()) {
                std::cout << "[Bucle infinito detectado en " << hex(sim.get_pc())
                          << " (fin del programa)]\n";
                break;
            }
            if (breakpoints.count(sim.get_pc())) {
                std::cout << "[Breakpoint en " << hex(sim.get_pc()) << "]\n";
                break;
            }
            if (i + 1 == max)
                std::cout << "[Se alcanzó el límite de " << max << " instrucciones]\n";
        }
        std::cout << "Ejecutadas " << sim.get_instret() - start
                  << " instrucciones. pc = " << hex(sim.get_pc()) << "\n";
    }

    void cmd_regs(const std::vector<std::string>& args) {
        if (args.size() == 1) {
            printf("pc         = %s\n", hex(sim.get_pc()).c_str());
            for (int i = 0; i < 32; i++)
                print_reg(i);
            return;
        }
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "pc") {
                printf("pc         = %s\n", hex(sim.get_pc()).c_str());
                continue;
            }
            int r = isa::parse_register(args[i]);
            if (r < 0)
                std::cout << "Registro desconocido: " << args[i] << "\n";
            else
                print_reg(r);
        }
    }

    void cmd_mem(const std::vector<std::string>& args) {
        uint32_t start, end;
        if (args.size() < 2 || !parse_addr(args[1], start)) {
            std::cout << "Uso: mem <inicio> [fin]\n";
            return;
        }
        end = start > 0xFFFFFFF0u ? 0xFFFFFFFFu : start + 15;
        if (args.size() > 2 && !parse_addr(args[2], end)) {
            std::cout << "Uso: mem <inicio> [fin]\n";
            return;
        }
        if (end < start) {
            std::cout << "El fin debe ser mayor o igual al inicio.\n";
            return;
        }
        if (end - start > 0xFFFF)
            end = start + 0xFFFF;

        const Memory& m = sim.memory();
        if (end - start < 16) {
            printf("Memoria (%s-%s):", hex(start).c_str(), hex(end).c_str());
            for (uint64_t a = start; a <= end; a++)
                printf(" %02X", m.read8(static_cast<uint32_t>(a)));
            printf("\n");
            return;
        }
        for (uint64_t row = start & ~0xFull; row <= end; row += 16) {
            printf("%s: ", hex(static_cast<uint32_t>(row)).c_str());
            std::string ascii;
            for (uint64_t a = row; a < row + 16; a++) {
                if (a < start || a > end) {
                    printf("   ");
                    ascii += ' ';
                    continue;
                }
                uint8_t b = m.read8(static_cast<uint32_t>(a));
                printf("%02X ", b);
                ascii += (b >= 32 && b < 127) ? static_cast<char>(b) : '.';
            }
            printf(" |%s|\n", ascii.c_str());
        }
    }

    void cmd_words(const std::vector<std::string>& args) {
        uint32_t addr, n = 8;
        if (args.size() < 2 || !parse_addr(args[1], addr) ||
            (args.size() > 2 && !parse_u32(args[2], n))) {
            std::cout << "Uso: words <dirección> [cantidad]\n";
            return;
        }
        for (uint32_t i = 0; i < n; i++) {
            uint32_t a = addr + 4 * i;
            uint32_t v = sim.memory().read32(a);
            printf("%s: %s  (%d)\n", hex(a).c_str(), hex(v).c_str(), static_cast<int32_t>(v));
        }
    }

    void cmd_disasm(const std::vector<std::string>& args) {
        uint32_t addr = sim.get_pc(), n = 10;
        if ((args.size() > 1 && !parse_addr(args[1], addr)) ||
            (args.size() > 2 && !parse_u32(args[2], n))) {
            std::cout << "Uso: disasm [dirección] [cantidad]\n";
            return;
        }
        addr &= ~3u;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t a = addr + 4 * i;
            std::cout << (a == sim.get_pc() ? "=> " : "   ") << (breakpoints.count(a) ? "* " : "  ")
                      << disasm_at(a) << "\n";
        }
    }

    void cmd_set(const std::vector<std::string>& args) {
        uint32_t v;
        if (args.size() != 3 || !parse_u32(args[2], v)) {
            std::cout << "Uso: set <registro|pc> <valor>\n";
            return;
        }
        if (args[1] == "pc") {
            sim.set_pc(v);
            return;
        }
        int r = isa::parse_register(args[1]);
        if (r < 0)
            std::cout << "Registro desconocido: " << args[1] << "\n";
        else if (r == 0)
            std::cout << "x0 siempre vale 0.\n";
        else
            sim.set_reg(r, v);
    }

    void cmd_poke(const std::vector<std::string>& args) {
        uint32_t addr, v;
        if (args.size() != 3 || !parse_addr(args[1], addr) || !parse_u32(args[2], v)) {
            std::cout << "Uso: poke <dirección> <word>\n";
            return;
        }
        sim.memory().write32(addr, v);
    }

    void cmd_break(const std::vector<std::string>& args, bool add) {
        uint32_t addr;
        if (args.size() != 2 || !parse_addr(args[1], addr)) {
            std::cout << (add ? "Uso: break <dirección>\n" : "Uso: delete <dirección>\n");
            return;
        }
        if (add) {
            breakpoints.insert(addr);
            std::cout << "Breakpoint en " << hex(addr) << "\n";
        } else {
            std::cout << (breakpoints.erase(addr) ? "Breakpoint eliminado.\n"
                                                  : "No había breakpoint ahí.\n");
        }
    }

    void cmd_info() {
        std::cout << "pc = " << hex(sim.get_pc()) << ", instrucciones ejecutadas = "
                  << sim.get_instret() << ", estado = "
                  << (sim.is_exited()      ? "terminado"
                      : sim.in_self_loop() ? "en bucle final"
                                           : "en ejecución")
                  << "\n";
        std::cout << "Breakpoints:";
        if (breakpoints.empty())
            std::cout << " (ninguno)";
        for (uint32_t b : breakpoints)
            std::cout << " " << hex(b);
        std::cout << "\nTrace: " << (trace ? "on" : "off") << "\n";
    }

    static void cmd_help() {
        std::cout <<
            "Comandos:\n"
            "  load <archivo>          Carga un binario crudo (en 0x0) o un ELF RV32\n"
            "  reset                   Reinicia registros, pc y memoria del programa\n"
            "  step [n]   (s)          Ejecuta n instrucciones (por defecto 1)\n"
            "  run [max]  (c)          Ejecuta hasta terminar, ebreak, breakpoint o bucle final\n"
            "  pc                      Muestra el PC\n"
            "  regs [r1 r2 ...]  (r)   Muestra registros (todos si no se indican)\n"
            "  mem <ini> [fin]   (m)   Muestra bytes de memoria (ini/fin pueden ser registros)\n"
            "  words <dir> [n]   (w)   Muestra n words de 32 bits desde dir\n"
            "  disasm [dir] [n]  (d)   Desensambla n instrucciones (por defecto desde el pc)\n"
            "  break <dir> / delete <dir>   Pone / quita un breakpoint\n"
            "  set <reg|pc> <valor>    Modifica un registro o el pc\n"
            "  poke <dir> <word>       Escribe un word en memoria\n"
            "  trace on|off            Muestra cada instrucción ejecutada en run\n"
            "  info                    Estado del simulador\n"
            "  help (h)                Esta ayuda\n"
            "  exit / quit (q)         Salir\n"
            "Una línea vacía repite el último comando (útil para step).\n";
    }

    bool execute(const std::string& line) {
        std::istringstream iss(line);
        std::vector<std::string> args;
        for (std::string w; iss >> w;)
            args.push_back(w);
        if (args.empty())
            return true;
        const std::string& c = args[0];

        if (c == "exit" || c == "quit" || c == "q")
            return false;
        else if (c == "help" || c == "h")
            cmd_help();
        else if (c == "load" && args.size() == 2)
            cmd_load(args[1]);
        else if (c == "reset") {
            if (require_program()) {
                std::string err;
                if (sim.load_from_file(current_file, err))
                    std::cout << "Simulador reiniciado.\n";
                else
                    std::cout << "Error al recargar: " << err << "\n";
            }
        } else if (c == "step" || c == "s")
            cmd_step(args);
        else if (c == "run" || c == "c" || c == "continue")
            cmd_run(args);
        else if (c == "pc")
            std::cout << "pc = " << hex(sim.get_pc()) << "\n";
        else if (c == "regs" || c == "r")
            cmd_regs(args);
        else if (c == "mem" || c == "m")
            cmd_mem(args);
        else if (c == "words" || c == "w")
            cmd_words(args);
        else if (c == "disasm" || c == "d")
            cmd_disasm(args);
        else if (c == "break" || c == "b")
            cmd_break(args, true);
        else if (c == "delete")
            cmd_break(args, false);
        else if (c == "set")
            cmd_set(args);
        else if (c == "poke")
            cmd_poke(args);
        else if (c == "trace" && args.size() == 2)
            trace = args[1] == "on";
        else if (c == "info")
            cmd_info();
        else
            std::cout << "Comando desconocido: \"" << line << "\". Escribe help.\n";
        return true;
    }
};

static void usage(const char* prog) {
    std::cerr << "Uso: " << prog << " [opciones] [programa.bin|programa.elf]\n"
              << "Opciones:\n"
              << "  --run           Ejecuta el programa completo sin REPL y muestra los registros\n"
              << "  --trace         Con --run, muestra cada instrucción ejecutada\n"
              << "  --max N         Límite de instrucciones para --run (por defecto 10000000)\n"
              << "  --dump A:B      Con --run, muestra la memoria de A a B al terminar\n";
}

int main(int argc, char** argv) {
    Repl repl;
    std::string file;
    bool batch = false;
    uint32_t max = DEFAULT_MAX_STEPS;
    std::vector<std::string> dumps;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--run")
            batch = true;
        else if (a == "--trace")
            repl.trace = true;
        else if (a == "--max" && i + 1 < argc)
            Repl::parse_u32(argv[++i], max);
        else if (a == "--dump" && i + 1 < argc)
            dumps.push_back(argv[++i]);
        else if (a == "-h" || a == "--help") {
            usage(argv[0]);
            return 0;
        } else if (!a.empty() && a[0] == '-') {
            usage(argv[0]);
            return 1;
        } else
            file = a;
    }

    if (!file.empty()) {
        repl.cmd_load(file);
        if (!repl.loaded)
            return 1;
    } else if (batch) {
        usage(argv[0]);
        return 1;
    }

    if (batch) {
        repl.cmd_run({"run", std::to_string(max)});
        repl.cmd_regs({"regs"});
        for (const auto& d : dumps) {
            auto colon = d.find(':');
            std::cout << "\n";
            if (colon == std::string::npos)
                repl.cmd_mem({"mem", d});
            else
                repl.cmd_mem({"mem", d.substr(0, colon), d.substr(colon + 1)});
        }
        return repl.sim.get_error().empty() ? 0 : 2;
    }

    repl.interactive = isatty(STDIN_FILENO);
    std::cout << "Simulador RV32I. Escribe \"help\" para ver los comandos.\n";
    std::string line, last;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line))
            break;
        if (!repl.interactive)
            std::cout << line << "\n";
        if (line.find_first_not_of(" \t") == std::string::npos)
            line = last;
        else
            last = line;
        if (!repl.execute(line))
            break;
    }
    std::cout << "See you next time...\n";
    std::cout << "Program exited with code " << repl.sim.get_exit_code() << ".\n";
    return 0;
}
