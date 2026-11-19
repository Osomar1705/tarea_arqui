#pragma once

#include <cstdint>
#include <string>

namespace isa {
    enum Opcode : uint32_t {
        OP_LOAD = 0x03,
        OP_MISC_MEM = 0x0F,
        OP_IMM = 0x13,
        OP_AUIPC = 0x17,
        OP_STORE = 0x23,
        OP_REG = 0x33,
        OP_LUI = 0x37,
        OP_BRANCH = 0x63,
        OP_JALR = 0x67,
        OP_JAL = 0x6F,
        OP_SYSTEM = 0x73,
    };

    inline uint32_t opcode(uint32_t instr) {
        return instr & 0x7F;
    }
    inline uint32_t rd(uint32_t instr) {
        return (instr >> 7) & 0x1F;
    }
    inline uint32_t funct3(uint32_t instr) {
        return (instr >> 12) & 0x7;
    }
    inline uint32_t rs1(uint32_t instr) {
        return (instr >> 15) & 0x1F;
    }
    inline uint32_t rs2(uint32_t instr) {
        return (instr >> 20) & 0x1F;
    }
    inline uint32_t funct7(uint32_t instr) {
        return (instr >> 25) & 0x7F;
    }

    inline int32_t imm_i(uint32_t instr) {
        return static_cast<int32_t>(instr) >> 20;
    }
    inline int32_t imm_s(uint32_t instr) {
        return (static_cast<int32_t>(instr & 0xFE000000) >> 20)
               | ((instr >> 7) & 0x1F);
    }
    inline int32_t imm_b(uint32_t instr) {
        return (static_cast<int32_t>(instr & 0x80000000) >> 19)
               | ((instr & 0x80) << 4)
               | ((instr >> 20) & 0x7E0)
               | ((instr >> 7) & 0x1E);
    }
    inline int32_t imm_u(uint32_t instr) {
        return static_cast<int32_t>(instr & 0xFFFFF000);
    }
    inline int32_t imm_j(uint32_t instr) {
        return (static_cast<int32_t>(instr & 0x80000000) >> 11)
               | (instr & 0xFF000)
               | ((instr >> 9) & 0x800)
               | ((instr >> 20) & 0x7FE);
    }

    extern const char* const ABI_NAMES[32];

    int parse_register(const std::string& name);

    std::string disassemble(uint32_t instr, uint32_t pc);
}
