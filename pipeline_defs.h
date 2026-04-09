// --- START OF FILE pipeline_defs.h ---
#ifndef PIPELINE_DEFS_H
#define PIPELINE_DEFS_H

#include "riscv_utils.h"

enum class ALUOps : uint8_t {
    ADD, SUB, AND, OR, XOR, SLT, SLTU, SLL, SRL, SRA,
    MUL, MULH, MULHSU, MULHU, DIV, DIVU, REM, REMU,
    PASSB, PASSPC
};

enum class FPUOps : uint8_t {
    FADD_S, FSUB_S, FMUL_S, FDIV_S, FSQRT_S, FSGNJ_S, FMINMAX_S,
    FCVT_W_S, FCVT_S_W, FCMP_S, FCLASS_S, FMV_X_W, FMV_W_X,
    FMADD_S, FMSUB_S, FNMSUB_S, FNMADD_S,
    UNKNOWN 
};

struct ControlSignals {
    bool RegWrite = false;
    bool MemRead = false;
    bool MemWrite = false;
    bool MemToReg = false;
    bool ALUSrc = false;
    ALUOps ALUOperation = ALUOps::ADD;
    bool Jump = false;
    bool Branch = false;
    bool fpRegWrite = false;
    bool fpMemRead = false;
    bool fpMemWrite = false;
    bool fpMemToReg = false;
    bool fpResultIsInt = false;
    FPUOps FPUOperation = FPUOps::UNKNOWN; // Default to unknown
    bool isFP = false;
    bool usesRS1 = false;
    bool usesRS2 = false;
    bool usesFS1 = false;
    bool usesFS2 = false;
    bool usesFS3 = false;
    bool writesIntRd = false;
    bool writesFpFd = false;
};

struct IFID_Reg {
    uint32 pc = 0;
    uint32 instruction = NOP_INSTRUCTION;
};

struct IDEX_Reg {
    uint32 pc = 0;
    uint32 raw_instruction = NOP_INSTRUCTION;
    std::string disasm_instr = "nop";
    int32 read_data1 = 0;
    int32 read_data2 = 0;
    float read_data_fp1 = 0.0f;
    float read_data_fp2 = 0.0f;
    float read_data_fp3 = 0.0f;
    int32 immediate = 0;
    uint32 rs1 = 0;
    uint32 rs2 = 0;
    uint32 rs3 = 0;
    uint32 rd = 0;
    ControlSignals control{};
};

struct EXMEM_Reg {
    uint32 pc = 0;
    uint32 raw_instruction = NOP_INSTRUCTION;
    std::string disasm_instr = "nop";
    uint32 pc_branch_target = 0;
    bool branch_taken = false;
    int32 alu_result = 0;
    float fp_alu_result = 0.0f;
    int32 store_data_int = 0;
    float store_data_fp = 0.0f;
    uint32 rd = 0;
    ControlSignals control{};
};

struct MEMWB_Reg {
    uint32 raw_instruction = NOP_INSTRUCTION;
    std::string disasm_instr = "nop";
    int32 mem_read_data_int = 0;
    float mem_read_data_fp = 0.0f;
    int32 alu_result = 0;
    float fp_alu_result = 0.0f;
    uint32 rd = 0;
    ControlSignals control{};
};

#endif 