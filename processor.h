#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "riscv_utils.h"
#include "pipeline_defs.h"
#include <vector>
#include <array>
#include <string>

class Processor {
public:
    uint32 pc = 0;
    std::array<int32, 32> regs{};
    std::array<float, 32> fregs{};
    std::vector<uint32> instruction_memory;
    std::vector<uint8> data_memory;

    IFID_Reg if_id_reg{};
    IDEX_Reg id_ex_reg{};
    EXMEM_Reg ex_mem_reg{};
    MEMWB_Reg mem_wb_reg{};

    IFID_Reg if_id_write_reg{};
    IDEX_Reg id_ex_write_reg{};
    EXMEM_Reg ex_mem_write_reg{};
    MEMWB_Reg mem_wb_write_reg{};

    bool stall_if = false;
    bool flush_ifid = false;
    bool flush_idex = false;

    uint64_t cycle_count = 0;
    bool halted = false;
    uint32 pc_next_update = 0;

    uint64_t branch_count = 0;
    uint64_t branch_taken = 0;
    uint64_t load_stall_count = 0;
    uint64_t control_hazard_count = 0;

    Processor();
    ~Processor() = default;

    void reset();
    void load_program(const std::vector<uint32>& program);
    bool step();
    void run_until_halt(uint64_t max_cycles = 1000000);

    void print_pipeline_state();
    void dump_registers();
    void dump_fregisters();
    void dump_memory(uint32 start_addr, uint32 words_to_dump);
    void print_performance_counters();

    bool is_halted() const;
    uint64_t get_cycle_count() const;
    size_t get_memory_size() const;
    size_t get_instruction_memory_size() const;

private:
    void fetch();
    void decode();
    void execute();
    void memory_access();
    void writeback();
    void detect_hazards();
    void tick();
};

#endif 