#pragma once

#include <cstdint>
#include <string>

#include "memory.hpp"

enum class StepResult {
    Ok,
    Exited,
    Breakpoint,
    Error,
};

class Simulator {
public:
    Simulator();

    bool load_raw(const std::string& filename, std::string& err);
    bool load_elf(const std::string& filename, std::string& err);
    bool load_from_file(const std::string& filename, std::string& err);

    void reset();

    StepResult step();

    uint32_t get_pc() const {
        return pc;
    }
    void set_pc(uint32_t value) {
        pc = value;
    }
    uint32_t get_reg(int i) const {
        return regs[i];
    }
    void set_reg(int i, uint32_t value) {
        if (i != 0)
            regs[i] = value;
    }

    Memory& memory() {
        return mem;
    }
    const Memory& memory() const {
        return mem;
    }

    uint64_t get_instret() const {
        return instret;
    }
    bool is_exited() const {
        return exited;
    }
    int get_exit_code() const {
        return exit_code;
    }
    const std::string& get_error() const {
        return error;
    }
    bool in_self_loop() const {
        return self_loop;
    }
private:
    StepResult ecall();
    StepResult fail(const std::string& msg);

    Memory mem;
    uint32_t pc = 0;
    uint32_t regs[32] = {};

    std::string loaded_file;
    uint32_t entry = 0;
    uint32_t program_end = 0;

    uint64_t instret = 0;
    bool exited = false;
    int exit_code = 0;
    bool self_loop = false;
    uint32_t heap_ptr = 0;
    std::string error;
};
