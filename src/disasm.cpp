#include <cstdarg>
#include <cstdio>
#include <string>

#include "isa.hpp"

namespace isa {
    const char* const ABI_NAMES[32] = {
        "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
    };

    int parse_register(const std::string& name) {
        if (name == "fp")
            return 8;
        for (int i = 0; i < 32; i++)
            if (name == ABI_NAMES[i])
                return i;
        if (name.size() >= 2 && (name[0] == 'x' || name[0] == 'X')) {
            int n = 0;
            for (size_t i = 1; i < name.size(); i++) {
                if (name[i] < '0' || name[i] > '9')
                    return -1;
                n = n * 10 + (name[i] - '0');
            }
            if (n < 32)
                return n;
        }
        return -1;
    }

    static std::string fmt(const char* f, ...) {
        char buf[128];
        va_list args;
        va_start(args, f);
        vsnprintf(buf, sizeof(buf), f, args);
        va_end(args);
        return buf;
    }

    std::string disassemble(uint32_t instr, uint32_t pc) {
        const char* const* R = ABI_NAMES;
        uint32_t op = opcode(instr), f3 = funct3(instr), f7 = funct7(instr);
        uint32_t d = rd(instr), s1 = rs1(instr), s2 = rs2(instr);

        switch (op) {
            case OP_LUI:
                return fmt("lui %s, 0x%x", R[d], static_cast<uint32_t>(imm_u(instr)) >> 12);
            case OP_AUIPC:
                return fmt("auipc %s, 0x%x", R[d], static_cast<uint32_t>(imm_u(instr)) >> 12);
            case OP_JAL: {
                uint32_t target = pc + imm_j(instr);
                if (d == 0)
                    return fmt("j 0x%x", target);
                if (d == 1)
                    return fmt("jal 0x%x", target);
                return fmt("jal %s, 0x%x", R[d], target);
            }
            case OP_JALR:
                if (d == 0 && s1 == 1 && imm_i(instr) == 0)
                    return "ret";
                return fmt("jalr %s, %d(%s)", R[d], imm_i(instr), R[s1]);
            case OP_BRANCH: {
                static const char* names[8] = {"beq", "bne", nullptr, nullptr,
                                               "blt", "bge", "bltu",  "bgeu"};
                if (!names[f3])
                    break;
                return fmt("%s %s, %s, 0x%x", names[f3], R[s1], R[s2], pc + imm_b(instr));
            }
            case OP_LOAD: {
                static const char* names[8] = {"lb",  "lh",  "lw",    nullptr,
                                               "lbu", "lhu", nullptr, nullptr};
                if (!names[f3])
                    break;
                return fmt("%s %s, %d(%s)", names[f3], R[d], imm_i(instr), R[s1]);
            }
            case OP_STORE: {
                static const char* names[8] = {"sb", "sh", "sw"};
                if (f3 > 2)
                    break;
                return fmt("%s %s, %d(%s)", names[f3], R[s2], imm_s(instr), R[s1]);
            }
            case OP_IMM: {
                int32_t imm = imm_i(instr);
                switch (f3) {
                    case 0:
                        if (instr == 0x00000013)
                            return "nop";
                        if (s1 == 0)
                            return fmt("li %s, %d", R[d], imm);
                        if (imm == 0)
                            return fmt("mv %s, %s", R[d], R[s1]);
                        return fmt("addi %s, %s, %d", R[d], R[s1], imm);
                    case 1:
                        return fmt("slli %s, %s, %u", R[d], R[s1], s2);
                    case 2:
                        return fmt("slti %s, %s, %d", R[d], R[s1], imm);
                    case 3:
                        return fmt("sltiu %s, %s, %d", R[d], R[s1], imm);
                    case 4:
                        return fmt("xori %s, %s, %d", R[d], R[s1], imm);
                    case 5:
                        return fmt("%s %s, %s, %u", (f7 & 0x20) ? "srai" : "srli", R[d], R[s1], s2);
                    case 6:
                        return fmt("ori %s, %s, %d", R[d], R[s1], imm);
                    case 7:
                        return fmt("andi %s, %s, %d", R[d], R[s1], imm);
                }
                break;
            }
            case OP_REG: {
                const char* name = nullptr;
                if (f7 == 0x00) {
                    static const char* names[8] = {"add", "sll", "slt", "sltu",
                                                   "xor", "srl", "or",  "and"};
                    name = names[f3];
                } else if (f7 == 0x20) {
                    if (f3 == 0)
                        name = "sub";
                    else if (f3 == 5)
                        name = "sra";
                }
                if (!name)
                    break;
                return fmt("%s %s, %s, %s", name, R[d], R[s1], R[s2]);
            }
            case OP_MISC_MEM:
                return "fence";
            case OP_SYSTEM:
                if (instr == 0x00000073)
                    return "ecall";
                if (instr == 0x00100073)
                    return "ebreak";
                break;
        }
        return fmt(".word 0x%08x  # ???", instr);
    }
}
