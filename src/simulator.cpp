#include "simulator.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

#include "isa.hpp"

using namespace isa;

Simulator::Simulator() {
    reset();
}

static bool read_file(const std::string& filename, std::vector<uint8_t>& data) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
        return false;
    data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool Simulator::load_raw(const std::string& filename, std::string& err) {
    std::vector<uint8_t> data;
    if (!read_file(filename, data)) {
        err = "no se pudo abrir \"" + filename + "\"";
        return false;
    }
    mem.clear();
    for (size_t i = 0; i < data.size(); i++)
        mem.write8(static_cast<uint32_t>(i), data[i]);

    loaded_file = filename;
    entry = 0;
    program_end = static_cast<uint32_t>(data.size());
    reset();
    return true;
}

bool Simulator::load_elf(const std::string& filename, std::string& err) {
    std::vector<uint8_t> d;
    if (!read_file(filename, d)) {
        err = "no se pudo abrir \"" + filename + "\"";
        return false;
    }
    auto u16 = [&](size_t o) { return static_cast<uint32_t>(d[o] | (d[o + 1] << 8)); };
    auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };

    if (d.size() < 52 || d[0] != 0x7F || d[1] != 'E' || d[2] != 'L' || d[3] != 'F') {
        err = "no es un archivo ELF";
        return false;
    }
    if (d[4] != 1 || d[5] != 1 || u16(18) != 243) {
        err = "el ELF no es RISC-V de 32 bits little-endian";
        return false;
    }

    uint32_t e_entry = u32(24), e_phoff = u32(28);
    uint32_t e_phentsize = u16(42), e_phnum = u16(44);

    mem.clear();
    program_end = 0;
    for (uint32_t i = 0; i < e_phnum; i++) {
        size_t ph = e_phoff + static_cast<size_t>(i) * e_phentsize;
        if (ph + 32 > d.size()) {
            err = "tabla de program headers truncada";
            return false;
        }
        if (u32(ph) != 1)
            continue;
        uint32_t offset = u32(ph + 4), vaddr = u32(ph + 8);
        uint32_t filesz = u32(ph + 16), memsz = u32(ph + 20);
        if (static_cast<size_t>(offset) + filesz > d.size()) {
            err = "segmento fuera del archivo";
            return false;
        }
        for (uint32_t j = 0; j < filesz; j++)
            mem.write8(vaddr + j, d[offset + j]);
        if (vaddr + memsz > program_end)
            program_end = vaddr + memsz;
    }

    loaded_file = filename;
    entry = e_entry;
    reset();
    return true;
}

bool Simulator::load_from_file(const std::string& filename, std::string& err) {
    std::vector<uint8_t> head;
    if (!read_file(filename, head)) {
        err = "no se pudo abrir \"" + filename + "\"";
        return false;
    }
    bool is_elf = head.size() >= 4 && head[0] == 0x7F && head[1] == 'E' && head[2] == 'L' &&
                  head[3] == 'F';
    return is_elf ? load_elf(filename, err) : load_raw(filename, err);
}

void Simulator::reset() {
    for (auto& r : regs)
        r = 0;
    regs[2] = 0x00100000;
    pc = entry;
    instret = 0;
    exited = false;
    exit_code = 0;
    self_loop = false;
    heap_ptr = 0;
    error.clear();
}

StepResult Simulator::fail(const std::string& msg) {
    char buf[64];
    snprintf(buf, sizeof(buf), " (pc = 0x%08X)", pc);
    error = msg + buf;
    return StepResult::Error;
}

StepResult Simulator::step() {
    if (exited)
        return StepResult::Exited;

    if (pc & 0x3)
        return fail("PC desalineado");
    uint32_t instr = mem.read32(pc);

    uint32_t op = opcode(instr);
    uint32_t d = rd(instr);
    uint32_t f3 = funct3(instr);
    uint32_t f7 = funct7(instr);
    uint32_t a = regs[rs1(instr)];
    uint32_t b = regs[rs2(instr)];
    int32_t sa = static_cast<int32_t>(a);
    int32_t sb = static_cast<int32_t>(b);

    uint32_t next_pc = pc + 4;
    uint32_t result = 0;
    bool write_rd = false;

    switch (op) {
        case OP_LUI:
            result = static_cast<uint32_t>(imm_u(instr));
            write_rd = true;
            break;

        case OP_AUIPC:
            result = pc + static_cast<uint32_t>(imm_u(instr));
            write_rd = true;
            break;

        case OP_JAL:
            result = pc + 4;
            write_rd = true;
            next_pc = pc + imm_j(instr);
            break;

        case OP_JALR:
            if (f3 != 0)
                return fail("instrucción ilegal");
            result = pc + 4;
            write_rd = true;
            next_pc = (a + imm_i(instr)) & ~1u;
            break;

        case OP_BRANCH: {
            bool taken;
            switch (f3) {
                case 0: taken = a == b; break;
                case 1: taken = a != b; break;
                case 4: taken = sa < sb; break;
                case 5: taken = sa >= sb; break;
                case 6: taken = a < b; break;
                case 7: taken = a >= b; break;
                default: return fail("instrucción de branch ilegal");
            }
            if (taken)
                next_pc = pc + imm_b(instr);
            break;
        }

        case OP_LOAD: {
            uint32_t addr = a + imm_i(instr);
            switch (f3) {
                case 0: result = static_cast<uint32_t>(static_cast<int8_t>(mem.read8(addr))); break;
                case 1: result = static_cast<uint32_t>(static_cast<int16_t>(mem.read16(addr))); break;
                case 2: result = mem.read32(addr); break;
                case 4: result = mem.read8(addr); break;
                case 5: result = mem.read16(addr); break;
                default: return fail("instrucción de load ilegal");
            }
            write_rd = true;
            break;
        }

        case OP_STORE: {
            uint32_t addr = a + imm_s(instr);
            switch (f3) {
                case 0: mem.write8(addr, b & 0xFF); break;
                case 1: mem.write16(addr, b & 0xFFFF); break;
                case 2: mem.write32(addr, b); break;
                default: return fail("instrucción de store ilegal");
            }
            break;
        }

        case OP_IMM: {
            int32_t imm = imm_i(instr);
            uint32_t shamt = rs2(instr);
            switch (f3) {
                case 0: result = a + imm; break;
                case 2: result = sa < imm ? 1 : 0; break;
                case 3: result = a < static_cast<uint32_t>(imm) ? 1 : 0; break;
                case 4: result = a ^ imm; break;
                case 6: result = a | imm; break;
                case 7: result = a & imm; break;
                case 1:
                    if (f7 != 0x00)
                        return fail("instrucción ilegal");
                    result = a << shamt;
                    break;
                case 5:
                    if (f7 == 0x00)
                        result = a >> shamt;
                    else if (f7 == 0x20)
                        result = static_cast<uint32_t>(sa >> shamt);
                    else
                        return fail("instrucción ilegal");
                    break;
            }
            write_rd = true;
            break;
        }

        case OP_REG: {
            uint32_t shamt = b & 0x1F;
            if (f7 == 0x00) {
                switch (f3) {
                    case 0: result = a + b; break;
                    case 1: result = a << shamt; break;
                    case 2: result = sa < sb ? 1 : 0; break;
                    case 3: result = a < b ? 1 : 0; break;
                    case 4: result = a ^ b; break;
                    case 5: result = a >> shamt; break;
                    case 6: result = a | b; break;
                    case 7: result = a & b; break;
                }
            } else if (f7 == 0x20 && f3 == 0) {
                result = a - b;
            } else if (f7 == 0x20 && f3 == 5) {
                result = static_cast<uint32_t>(sa >> shamt);
            } else {
                return fail("instrucción ilegal");
            }
            write_rd = true;
            break;
        }

        case OP_MISC_MEM:
            break;

        case OP_SYSTEM:
            if (instr == 0x00000073) {
                StepResult r = ecall();
                if (r != StepResult::Ok) {
                    if (r == StepResult::Exited) {
                        pc = next_pc;
                        instret++;
                    }
                    return r;
                }
                break;
            }
            if (instr == 0x00100073) {
                pc = next_pc;
                instret++;
                return StepResult::Breakpoint;
            }
            return fail("instrucción de sistema no soportada");

        default:
            char buf[64];
            snprintf(buf, sizeof(buf), "instrucción ilegal 0x%08X", instr);
            return fail(buf);
    }

    if (write_rd && d != 0)
        regs[d] = result;
    regs[0] = 0;

    self_loop = (next_pc == pc);
    pc = next_pc;
    instret++;
    return StepResult::Ok;
}

StepResult Simulator::ecall() {
    uint32_t a0 = regs[10], a1 = regs[11], a7 = regs[17];
    char buf[64];
    std::string line;

    switch (a7) {
        case 1:
            std::cout << static_cast<int32_t>(a0) << std::flush;
            break;
        case 4: {
            std::string s;
            for (uint32_t p = a0; s.size() < (1u << 20); p++) {
                char c = static_cast<char>(mem.read8(p));
                if (c == '\0')
                    break;
                s += c;
            }
            std::cout << s << std::flush;
            break;
        }
        case 5:
            regs[10] = std::getline(std::cin, line) ? static_cast<uint32_t>(std::strtol(line.c_str(), nullptr, 0)) : 0;
            break;
        case 8: {
            if (!std::getline(std::cin, line))
                line.clear();
            if (a1 == 0)
                break;
            uint32_t n = 0;
            for (; n + 1 < a1 && n < line.size(); n++)
                mem.write8(a0 + n, static_cast<uint8_t>(line[n]));
            if (n + 1 < a1)
                mem.write8(a0 + n++, '\n');
            mem.write8(a0 + n, 0);
            break;
        }
        case 9: {
            if (heap_ptr == 0)
                heap_ptr = std::max<uint32_t>((program_end + 0xFFF) & ~0xFFFu, 0x10000);
            regs[10] = heap_ptr;
            heap_ptr += (a0 + 3) & ~3u;
            break;
        }
        case 10:
            exited = true;
            exit_code = 0;
            return StepResult::Exited;
        case 11:
            std::cout << static_cast<char>(a0 & 0xFF) << std::flush;
            break;
        case 12: {
            int c = std::cin.get();
            regs[10] = c == EOF ? 0xFFFFFFFF : static_cast<uint32_t>(c);
            break;
        }
        case 17:
        case 93:
            exited = true;
            exit_code = static_cast<int32_t>(a0);
            return StepResult::Exited;
        case 30: {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
            regs[10] = static_cast<uint32_t>(ms);
            regs[11] = static_cast<uint32_t>(static_cast<uint64_t>(ms) >> 32);
            break;
        }
        case 32:
            std::this_thread::sleep_for(std::chrono::milliseconds(a0));
            break;
        case 34:
            snprintf(buf, sizeof(buf), "0x%08x", a0);
            std::cout << buf << std::flush;
            break;
        case 35: {
            std::string s;
            for (int i = 31; i >= 0; i--)
                s += ((a0 >> i) & 1) ? '1' : '0';
            std::cout << s << std::flush;
            break;
        }
        case 36:
            std::cout << a0 << std::flush;
            break;
        default:
            snprintf(buf, sizeof(buf), "ecall no soportada (a7 = %u)", a7);
            return fail(buf);
    }
    return StepResult::Ok;
}
