// --- processor.cpp --- (Continuing Execute Stage FPU Logic)

#include "processor.h"
#include "riscv_utils.h"
#include "pipeline_defs.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <limits>

Processor::Processor() : data_memory(MEMORY_SIZE, 0) { reset(); }
void Processor::reset() {
    pc = 0; std::fill(regs.begin(), regs.end(), 0); std::fill(fregs.begin(), fregs.end(), 0.0f); regs[2] = MEMORY_SIZE - 4;
    data_memory.assign(MEMORY_SIZE, 0); instruction_memory.clear();
    data_memory[0] = 10; data_memory[4] = 0xAA; data_memory[8] = 0xBB;
    if_id_reg = {}; if_id_reg.instruction = NOP_INSTRUCTION;
    id_ex_reg = {}; id_ex_reg.raw_instruction = NOP_INSTRUCTION; id_ex_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0);
    ex_mem_reg = {}; ex_mem_reg.raw_instruction = NOP_INSTRUCTION; ex_mem_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0);
    mem_wb_reg = {}; mem_wb_reg.raw_instruction = NOP_INSTRUCTION; mem_wb_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0);
    if_id_write_reg = if_id_reg; id_ex_write_reg = id_ex_reg; ex_mem_write_reg = ex_mem_reg; mem_wb_write_reg = mem_wb_reg;
    stall_if = false; flush_ifid = false; flush_idex = false; cycle_count = 0; halted = false; pc_next_update = 0;
    branch_count = 0; branch_taken = 0; load_stall_count = 0; control_hazard_count = 0;
}
void Processor::load_program(const std::vector<uint32>& program) {
    reset(); if (program.empty()) { instruction_memory.assign(INSTR_MEMORY_SIZE, HALT_INSTRUCTION); return; }
    if (program.size() > INSTR_MEMORY_SIZE) { halted = true; std::cerr << "Error: Program too large.\n"; return; }
    instruction_memory.assign(program.begin(), program.end()); instruction_memory.resize(INSTR_MEMORY_SIZE, HALT_INSTRUCTION);
}
void Processor::fetch() {
    if (halted || stall_if) {
	 if_id_write_reg = if_id_reg; return;
	  }
    if (pc % 4 != 0) { if (!halted) { halted = true; } if_id_write_reg.instruction = HALT_INSTRUCTION; }
    else if (pc >= instruction_memory.size() * 4) { if (!halted && pc >= INSTR_MEMORY_SIZE * 4) { } if_id_write_reg.instruction = HALT_INSTRUCTION; }
    else { if_id_write_reg.instruction = instruction_memory[pc / 4]; }
    if_id_write_reg.pc = pc;
}
void Processor::decode() {
    if (halted) { id_ex_write_reg = {}; id_ex_write_reg.raw_instruction = NOP_INSTRUCTION; id_ex_write_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0); return; }
    uint32 instr = if_id_reg.instruction; uint32 current_pc = if_id_reg.pc;
    id_ex_write_reg = {}; id_ex_write_reg.raw_instruction = instr; id_ex_write_reg.pc = current_pc; id_ex_write_reg.disasm_instr = disassemble(instr, current_pc);
    if (instr == NOP_INSTRUCTION || instr == HALT_INSTRUCTION) return;
    uint32 opcode=get_opcode(instr), funct3=get_funct3(instr), funct7=get_funct7(instr), rd_idx=get_rd(instr), rs1_idx=get_rs1(instr), rs2_idx=get_rs2(instr), rs3_idx=get_rs3(instr);
    id_ex_write_reg.rd=rd_idx; id_ex_write_reg.rs1=rs1_idx; id_ex_write_reg.rs2=rs2_idx; id_ex_write_reg.rs3=rs3_idx;
    regs[0]=0; id_ex_write_reg.read_data1=regs[rs1_idx]; id_ex_write_reg.read_data2=regs[rs2_idx]; id_ex_write_reg.read_data_fp1=fregs[rs1_idx]; id_ex_write_reg.read_data_fp2=fregs[rs2_idx]; id_ex_write_reg.read_data_fp3=fregs[rs3_idx];
    ControlSignals& ctrl = id_ex_write_reg.control; ctrl={}; ctrl.ALUOperation=ALUOps::ADD;
    switch(opcode){
        case 0x37: ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.ALUSrc=true; ctrl.ALUOperation=ALUOps::PASSB; id_ex_write_reg.immediate=get_imm_u(instr); break;
        case 0x17: ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.ALUSrc=true; ctrl.ALUOperation=ALUOps::PASSPC; id_ex_write_reg.immediate=get_imm_u(instr); break;
        case 0x6F: ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.Jump=true; ctrl.ALUOperation=ALUOps::ADD; id_ex_write_reg.immediate=get_imm_j(instr); break;
        case 0x67: if (funct3==0) { ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.Jump=true; ctrl.usesRS1=true; ctrl.ALUSrc=true; ctrl.ALUOperation=ALUOps::ADD; id_ex_write_reg.immediate=get_imm_i(instr); } else { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); } break;
        case 0x63: ctrl.Branch=true; ctrl.usesRS1=true; ctrl.usesRS2=true; ctrl.ALUSrc=false; ctrl.ALUOperation=ALUOps::SUB; id_ex_write_reg.immediate=get_imm_b(instr); if(funct3 != 0x0 && funct3 != 0x1 && funct3 != 0x4 && funct3 != 0x5 && funct3 != 0x6 && funct3 != 0x7){ ctrl = {}; id_ex_write_reg.raw_instruction = NOP_INSTRUCTION; id_ex_write_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0); } break;
        case 0x03: if (funct3==0x2) { ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.MemRead=true; ctrl.MemToReg=true; ctrl.usesRS1=true; ctrl.ALUSrc=true; ctrl.ALUOperation=ALUOps::ADD; id_ex_write_reg.immediate=get_imm_i(instr); } else { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); } break;
        case 0x23: if (funct3==0x2) { ctrl.MemWrite=true; ctrl.usesRS1=true; ctrl.usesRS2=true; ctrl.ALUSrc=true; ctrl.ALUOperation=ALUOps::ADD; id_ex_write_reg.immediate=get_imm_s(instr); } else { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); } break;
        case 0x13: ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.usesRS1=true; ctrl.ALUSrc=true; id_ex_write_reg.immediate=get_imm_i(instr); switch (funct3) { case 0: ctrl.ALUOperation=ALUOps::ADD; break; case 1: ctrl.ALUOperation=ALUOps::SLL; id_ex_write_reg.immediate=get_shamt_i(instr); break; case 2: ctrl.ALUOperation=ALUOps::SLT; break; case 3: ctrl.ALUOperation=ALUOps::SLTU; break; case 4: ctrl.ALUOperation=ALUOps::XOR; break; case 5: if(((instr>>30)&1)==0) ctrl.ALUOperation=ALUOps::SRL; else ctrl.ALUOperation=ALUOps::SRA; id_ex_write_reg.immediate=get_shamt_i(instr); break; case 6: ctrl.ALUOperation=ALUOps::OR; break; case 7: ctrl.ALUOperation=ALUOps::AND; break; default: ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); break; } break;
        case 0x33: ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.usesRS1=true; ctrl.usesRS2=true; ctrl.ALUSrc=false; if(funct7==0x01){ switch(funct3){ case 0:ctrl.ALUOperation=ALUOps::MUL; break; case 1:ctrl.ALUOperation=ALUOps::MULH; break; case 2:ctrl.ALUOperation=ALUOps::MULHSU; break; case 3:ctrl.ALUOperation=ALUOps::MULHU; break; case 4:ctrl.ALUOperation=ALUOps::DIV; break; case 5:ctrl.ALUOperation=ALUOps::DIVU; break; case 6:ctrl.ALUOperation=ALUOps::REM; break; case 7:ctrl.ALUOperation=ALUOps::REMU; break; default: ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); break; } } else { bool alt=(funct7==0x20); switch(funct3){ case 0:ctrl.ALUOperation=alt?ALUOps::SUB:ALUOps::ADD; break; case 1:ctrl.ALUOperation=ALUOps::SLL; break; case 2:ctrl.ALUOperation=ALUOps::SLT; break; case 3:ctrl.ALUOperation=ALUOps::SLTU; break; case 4:ctrl.ALUOperation=ALUOps::XOR; break; case 5:ctrl.ALUOperation=alt?ALUOps::SRA:ALUOps::SRL; break; case 6:ctrl.ALUOperation=ALUOps::OR; break; case 7:ctrl.ALUOperation=ALUOps::AND; break; default: ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); break; } } break;
        case 0x07: if (funct3 == 0x2) { ctrl.isFP=true; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.fpMemRead=true; ctrl.fpMemToReg=true; ctrl.usesRS1=true; ctrl.ALUSrc=true; ctrl.ALUOperation=ALUOps::ADD; id_ex_write_reg.immediate=get_imm_i(instr); } else { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0);} break;
        case 0x27: if (funct3 == 0x2) { ctrl.isFP=true; ctrl.fpMemWrite=true; ctrl.usesRS1=true; ctrl.usesFS2=true; ctrl.ALUSrc=true; ctrl.ALUOperation=ALUOps::ADD; id_ex_write_reg.immediate=get_imm_s(instr); } else { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0);} break;
        case 0x43: case 0x47: case 0x4B: case 0x4F: ctrl.isFP=true; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesFS1=true; ctrl.usesFS2=true; ctrl.usesFS3=true; if(opcode==0x43) ctrl.FPUOperation = FPUOps::FMADD_S; else if(opcode==0x47) ctrl.FPUOperation = FPUOps::FMSUB_S; else if(opcode==0x4B) ctrl.FPUOperation = FPUOps::FNMSUB_S; else ctrl.FPUOperation = FPUOps::FNMADD_S; break;
        case 0x53: ctrl.isFP=true; ctrl.usesFS1=true; switch(funct7) { case 0b0000000: ctrl.FPUOperation=FPUOps::FADD_S; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesFS2=true; break; case 0b0000100: ctrl.FPUOperation=FPUOps::FSUB_S; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesFS2=true; break; case 0b0001000: ctrl.FPUOperation=FPUOps::FMUL_S; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesFS2=true; break; case 0b0001100: ctrl.FPUOperation=FPUOps::FDIV_S; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesFS2=true; break; case 0b0101100: if(rs2_idx==0){ ctrl.FPUOperation=FPUOps::FSQRT_S; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; } else { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); } break; case 0b0010000: ctrl.FPUOperation=FPUOps::FSGNJ_S; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesFS2=true; break; case 0b0010100: ctrl.FPUOperation=FPUOps::FMINMAX_S; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesFS2=true; break; case 0b1100000: ctrl.FPUOperation=FPUOps::FCVT_W_S; ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.fpResultIsInt=true; break; case 0b1110000: if(funct3==0){ ctrl.FPUOperation=FPUOps::FMV_X_W; ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.fpResultIsInt=true;} else if(funct3==1){ ctrl.FPUOperation=FPUOps::FCLASS_S; ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.fpResultIsInt=true;} else { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); } break; case 0b1010000: ctrl.FPUOperation=FPUOps::FCMP_S; ctrl.RegWrite=true; ctrl.writesIntRd=true; ctrl.usesFS2=true; ctrl.fpResultIsInt=true; break; case 0b1101000: ctrl.FPUOperation=FPUOps::FCVT_S_W; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesRS1=true; ctrl.usesFS1=false; break; case 0b1111000: if(funct3==0){ ctrl.FPUOperation=FPUOps::FMV_W_X; ctrl.fpRegWrite=true; ctrl.writesFpFd=true; ctrl.usesRS1=true; ctrl.usesFS1=false;} else { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); } break; default: ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); break; } if (ctrl.writesIntRd) { ctrl.fpRegWrite=false; ctrl.writesFpFd=false; } else if (ctrl.writesFpFd) { ctrl.RegWrite=false; ctrl.writesIntRd=false; } else { if (id_ex_write_reg.raw_instruction != NOP_INSTRUCTION) { ctrl={}; id_ex_write_reg.raw_instruction=NOP_INSTRUCTION; id_ex_write_reg.disasm_instr=disassemble(NOP_INSTRUCTION,0); } } break;
        default: std::cerr << "Warning [DECODE]: Unknown opcode 0x" << std::hex << opcode << " at PC 0x" << current_pc << ". Treating as NOP.\n"; ctrl = {}; id_ex_write_reg = {}; id_ex_write_reg.raw_instruction = NOP_INSTRUCTION; id_ex_write_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0); break;
    }
}
void Processor::execute() {
    if (halted) { ex_mem_write_reg = {}; ex_mem_write_reg.raw_instruction = NOP_INSTRUCTION; ex_mem_write_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0); return; }
    ex_mem_write_reg = {}; ex_mem_write_reg.control = id_ex_reg.control; ex_mem_write_reg.rd = id_ex_reg.rd; ex_mem_write_reg.raw_instruction = id_ex_reg.raw_instruction; ex_mem_write_reg.pc = id_ex_reg.pc; ex_mem_write_reg.disasm_instr = id_ex_reg.disasm_instr;
    ControlSignals& ctrl = id_ex_reg.control;
    int32 forward_val1_int = id_ex_reg.read_data1; int32 forward_val2_int = id_ex_reg.read_data2; float forward_val1_fp = id_ex_reg.read_data_fp1; float forward_val2_fp = id_ex_reg.read_data_fp2; float forward_val3_fp = id_ex_reg.read_data_fp3;
    bool ex_mem_is_load = ex_mem_reg.control.MemRead || ex_mem_reg.control.fpMemRead;
    if (!ex_mem_is_load) { if (ex_mem_reg.control.RegWrite && ex_mem_reg.rd != 0) { if (ctrl.usesRS1 && ex_mem_reg.rd == id_ex_reg.rs1) forward_val1_int = ex_mem_reg.alu_result; if (ctrl.usesRS2 && ex_mem_reg.rd == id_ex_reg.rs2) forward_val2_int = ex_mem_reg.alu_result; } if (ex_mem_reg.control.fpRegWrite && ex_mem_reg.rd != 0) { if (ctrl.usesFS1 && ex_mem_reg.rd == id_ex_reg.rs1) forward_val1_fp = ex_mem_reg.fp_alu_result; if (ctrl.usesFS2 && ex_mem_reg.rd == id_ex_reg.rs2) forward_val2_fp = ex_mem_reg.fp_alu_result; if (ctrl.usesFS3 && ex_mem_reg.rd == id_ex_reg.rs3) forward_val3_fp = ex_mem_reg.fp_alu_result; } }
    if (mem_wb_reg.control.RegWrite && mem_wb_reg.rd != 0) { int32 wb_data_int = mem_wb_reg.control.MemToReg ? mem_wb_reg.mem_read_data_int : mem_wb_reg.alu_result; if (ctrl.usesRS1 && mem_wb_reg.rd == id_ex_reg.rs1) forward_val1_int = wb_data_int; if (ctrl.usesRS2 && mem_wb_reg.rd == id_ex_reg.rs2) forward_val2_int = wb_data_int; }
    if (mem_wb_reg.control.fpRegWrite && mem_wb_reg.rd != 0) { float wb_data_fp = mem_wb_reg.control.fpMemToReg ? mem_wb_reg.mem_read_data_fp : mem_wb_reg.fp_alu_result; if (ctrl.usesFS1 && mem_wb_reg.rd == id_ex_reg.rs1) forward_val1_fp = wb_data_fp; if (ctrl.usesFS2 && mem_wb_reg.rd == id_ex_reg.rs2) forward_val2_fp = wb_data_fp; if (ctrl.usesFS3 && mem_wb_reg.rd == id_ex_reg.rs3) forward_val3_fp = wb_data_fp; }
    int32 alu_in1 = forward_val1_int; int32 alu_in2 = ctrl.ALUSrc ? id_ex_reg.immediate : forward_val2_int; float fpu_in1 = forward_val1_fp; float fpu_in2 = forward_val2_fp; float fpu_in3 = forward_val3_fp;
    if (ctrl.isFP && (ctrl.FPUOperation == FPUOps::FCVT_S_W || ctrl.FPUOperation == FPUOps::FMV_W_X)) { fpu_in1 = bits_to_float(forward_val1_int); }
    int32 result_int = 0; float result_fp = 0.0f;

    if (ctrl.isFP) {
        uint32 instr_bits = id_ex_reg.raw_instruction; uint32 funct3 = get_funct3(instr_bits); uint32 rs2_idx = get_rs2(instr_bits);
        switch (ctrl.FPUOperation) {
            case FPUOps::FADD_S: result_fp = fpu_in1 + fpu_in2; break;
            case FPUOps::FSUB_S: result_fp = fpu_in1 - fpu_in2; break;
            case FPUOps::FMUL_S: result_fp = fpu_in1 * fpu_in2; break;
            case FPUOps::FMADD_S: result_fp = std::fma(fpu_in1, fpu_in2, fpu_in3); break;
            case FPUOps::FMSUB_S: result_fp = std::fma(fpu_in1, fpu_in2, -fpu_in3); break;
            case FPUOps::FNMADD_S: result_fp = std::fma(-fpu_in1, fpu_in2, -fpu_in3); break;
            case FPUOps::FNMSUB_S: result_fp = std::fma(-fpu_in1, fpu_in2, fpu_in3); break;
            case FPUOps::FDIV_S: result_fp = (fpu_in2 == 0.0f) ? std::copysign(INFINITY, fpu_in1) : (fpu_in1 / fpu_in2); break;
            case FPUOps::FSQRT_S: result_fp = (fpu_in1 < 0.0f && !std::isnan(fpu_in1)) ? NAN : std::sqrt(fpu_in1); break;
            case FPUOps::FSGNJ_S: switch(funct3){ case 0: result_fp = std::copysign(std::abs(fpu_in1), fpu_in2); break; case 1: result_fp = std::copysign(std::abs(fpu_in1), -fpu_in2); break; case 2: result_fp = std::copysign(std::abs(fpu_in1), bits_to_float(float_to_bits(fpu_in1)^float_to_bits(fpu_in2))); break; default: result_fp = NAN; break; } break;
            case FPUOps::FMINMAX_S: switch(funct3){ case 0: result_fp = (std::isnan(fpu_in1) || std::isnan(fpu_in2)) ? NAN : std::fmin(fpu_in1, fpu_in2); break; case 1: result_fp = (std::isnan(fpu_in1) || std::isnan(fpu_in2)) ? NAN : std::fmax(fpu_in1, fpu_in2); break; default: result_fp = NAN; break; } break;

            // --- Continuing FPU Cases ---
            case FPUOps::FCVT_W_S: 
                 if (rs2_idx == 0) { 
                     if (std::isnan(fpu_in1) || std::isinf(fpu_in1)) { /* TODO: Set Invalid flag (NV) */ result_int = std::numeric_limits<int32>::max();} 
                     else { result_int = static_cast<int32>(std::lrint(fpu_in1)); } 
                 } else { 
                     if (std::isnan(fpu_in1) || std::isinf(fpu_in1) || fpu_in1 < 0.0f) { /* TODO: Set NV flag */ result_int = std::numeric_limits<uint32>::max();}
                     else { result_int = static_cast<int32>(static_cast<uint32>(std::lrint(fpu_in1))); } 
                 }
                 break;
            case FPUOps::FCVT_S_W: 

                 if (rs2_idx == 0) { 
                     result_fp = static_cast<float>(float_to_bits(fpu_in1)); 
                 } else { 
                     result_fp = static_cast<float>(static_cast<uint32>(float_to_bits(fpu_in1)));
                 }
                 break;
            case FPUOps::FMV_X_W:
                 result_int = float_to_bits(fpu_in1); break;
            case FPUOps::FMV_W_X: 
                 result_fp = fpu_in1; break; 
            case FPUOps::FCLASS_S: 
                 { int c = std::fpclassify(fpu_in1); uint32 mask = 0;
                   if (c == FP_INFINITE)  mask = (std::signbit(fpu_in1) ? (1 << 0) : (1 << 7));
                   else if (c == FP_NAN) mask = (/* is signaling? */ (1 << 9) /* : (1 << 8) */);   
                   else if (c == FP_ZERO)   mask = (std::signbit(fpu_in1) ? (1 << 3) : (1 << 4));
                   else if (c == FP_SUBNORMAL) mask = (std::signbit(fpu_in1) ? (1 << 2) : (1 << 5));
                   else if (c == FP_NORMAL) mask = (std::signbit(fpu_in1) ? (1 << 1) : (1 << 6));
                   result_int = static_cast<int32>(mask);
                 } break;

                 switch(funct3) {
                     case 0x0: result_int = (!std::isnan(fpu_in1) && !std::isnan(fpu_in2) && fpu_in1 < fpu_in2) ? 1 : 0; break; 
                     case 0x1: result_int = (!std::isnan(fpu_in1) && !std::isnan(fpu_in2) && fpu_in1 <= fpu_in2) ? 1 : 0; break; 
                     case 0x2: result_int = (!std::isnan(fpu_in1) && !std::isnan(fpu_in2) && fpu_in1 == fpu_in2) ? 1 : 0; break; 
                     default: result_int = 0; break;
                 } break;
            case FPUOps::UNKNOWN: 
                 std::cerr << "Error [EXECUTE]: Encountered unknown FPU operation. Treating as NOP.\n";
                 ctrl = {}; ex_mem_write_reg.control = {}; result_fp = 0.0f; result_int = 0;
                 break;
        }
        if (ctrl.fpResultIsInt) { ex_mem_write_reg.alu_result = result_int; }
        else { ex_mem_write_reg.fp_alu_result = result_fp; } 

    } else { 
        uint32 shamt = 0;
        if (ctrl.ALUOperation >= ALUOps::SLL && ctrl.ALUOperation <= ALUOps::SRA) {
             shamt = ctrl.ALUSrc ? static_cast<uint32>(id_ex_reg.immediate & 0x1F) : static_cast<uint32>(alu_in2 & 0x1F);
        }
        switch (ctrl.ALUOperation) {
            case ALUOps::ADD:    result_int = alu_in1 + alu_in2; break;
            case ALUOps::SUB:    result_int = alu_in1 - alu_in2; break;
            case ALUOps::AND:    result_int = alu_in1 & alu_in2; break;
            case ALUOps::OR:     result_int = alu_in1 | alu_in2; break;
            case ALUOps::XOR:    result_int = alu_in1 ^ alu_in2; break;
            case ALUOps::SLT:    result_int = (alu_in1 < alu_in2) ? 1 : 0; break;
            case ALUOps::SLTU:   result_int = (static_cast<uint32>(alu_in1) < static_cast<uint32>(alu_in2)) ? 1 : 0; break;
            case ALUOps::SLL:    result_int = alu_in1 << shamt; break;
            case ALUOps::SRL:    result_int = static_cast<uint32>(alu_in1) >> shamt; break;
            case ALUOps::SRA:    result_int = alu_in1 >> shamt; break;
            case ALUOps::PASSB:  result_int = alu_in2; break;
            case ALUOps::PASSPC: result_int = id_ex_reg.pc + alu_in2; break;
            case ALUOps::MUL:    result_int = static_cast<int32>((static_cast<int64>(alu_in1) * static_cast<int64>(alu_in2)) & 0xFFFFFFFF); break;
            case ALUOps::MULH:   result_int = static_cast<int32>((static_cast<int64>(alu_in1) * static_cast<int64>(alu_in2)) >> 32); break;
            case ALUOps::MULHSU: result_int = static_cast<int32>((static_cast<int64>(alu_in1) * static_cast<uint64>(static_cast<uint32>(alu_in2))) >> 32); break;
            case ALUOps::MULHU:  result_int = static_cast<int32>((static_cast<uint64>(static_cast<uint32>(alu_in1)) * static_cast<uint64>(static_cast<uint32>(alu_in2))) >> 32); break;
            case ALUOps::DIV:    if(alu_in2==0) result_int=-1; else if(alu_in1==std::numeric_limits<int32>::min()&&alu_in2==-1) result_int=alu_in1; else result_int=alu_in1/alu_in2; break;
            case ALUOps::DIVU:   if(alu_in2==0) result_int=std::numeric_limits<int32>::max(); /*0xFFFFFFFF*/ else result_int=static_cast<int32>(static_cast<uint32>(alu_in1)/static_cast<uint32>(alu_in2)); break; // Match spec for unsigned div by zero
            case ALUOps::REM:    if(alu_in2==0) result_int=alu_in1; else if(alu_in1==std::numeric_limits<int32>::min()&&alu_in2==-1) result_int=0; else result_int=alu_in1%alu_in2; break;
            case ALUOps::REMU:   if(alu_in2==0) result_int=alu_in1; else result_int=static_cast<int32>(static_cast<uint32>(alu_in1)%static_cast<uint32>(alu_in2)); break;
            default: std::cerr << "Error [EXECUTE]: Unhandled ALU Operation (" << static_cast<int>(ctrl.ALUOperation) << ") encountered.\n"; result_int = 0; break;
        }
        ex_mem_write_reg.alu_result = result_int;
    }

    ex_mem_write_reg.branch_taken = false;
    uint32 calculated_target_pc = 0;
    bool perform_jump_or_branch = false;

    if (ctrl.Branch) {
        bool condition_met = false;
        uint32 current_funct3 = get_funct3(id_ex_reg.raw_instruction);
        switch (current_funct3) { 
            case 0x0: condition_met=(forward_val1_int == forward_val2_int); break; 
            case 0x1: condition_met=(forward_val1_int != forward_val2_int); break;
            case 0x4: condition_met=(forward_val1_int < forward_val2_int); break; 
            case 0x5: condition_met=(forward_val1_int >= forward_val2_int); break; 
            case 0x6: condition_met=(static_cast<uint32>(forward_val1_int) < static_cast<uint32>(forward_val2_int)); break;
            case 0x7: condition_met=(static_cast<uint32>(forward_val1_int) >= static_cast<uint32>(forward_val2_int)); break;
        }
        branch_count++; 
        if (condition_met) {
            calculated_target_pc = id_ex_reg.pc + id_ex_reg.immediate; 
            perform_jump_or_branch = true;
            branch_taken++; 
        }
    } else if (ctrl.Jump) { 
        perform_jump_or_branch = true;
        branch_count++;
        branch_taken++; 

        if (get_opcode(id_ex_reg.raw_instruction) == 0x6F) { 
            calculated_target_pc = id_ex_reg.pc + id_ex_reg.immediate;
            ex_mem_write_reg.alu_result = id_ex_reg.pc + 4;
        } else { 
            calculated_target_pc = (forward_val1_int + id_ex_reg.immediate) & ~1U; 
            ex_mem_write_reg.alu_result = id_ex_reg.pc + 4; 
        }
    }

    if (perform_jump_or_branch) {
        ex_mem_write_reg.branch_taken = true;
        ex_mem_write_reg.pc_branch_target = calculated_target_pc;
    }

    ex_mem_write_reg.store_data_int = forward_val2_int;
    ex_mem_write_reg.store_data_fp = forward_val2_fp;
}

void Processor::memory_access() {
    if (halted) { mem_wb_write_reg = {}; mem_wb_write_reg.raw_instruction = NOP_INSTRUCTION; mem_wb_write_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0); return; }
    mem_wb_write_reg = {}; mem_wb_write_reg.control = ex_mem_reg.control; mem_wb_write_reg.rd = ex_mem_reg.rd; mem_wb_write_reg.alu_result = ex_mem_reg.alu_result; mem_wb_write_reg.fp_alu_result = ex_mem_reg.fp_alu_result; mem_wb_write_reg.raw_instruction = ex_mem_reg.raw_instruction; mem_wb_write_reg.disasm_instr = ex_mem_reg.disasm_instr;
    ControlSignals& ctrl = ex_mem_reg.control;
    if (ex_mem_reg.branch_taken) { pc_next_update = ex_mem_reg.pc_branch_target; control_hazard_count++; }
    uint32 address = static_cast<uint32>(ex_mem_reg.alu_result); bool memory_access_error = false; uint32 access_size = 4;
    if (ctrl.MemRead || ctrl.MemWrite || ctrl.fpMemRead || ctrl.fpMemWrite) { access_size = 4; if (address % access_size != 0) { std::cerr << "Error [MEM]: Misaligned Mem access Addr=0x" << std::hex << address << " Size=" << access_size << " PC=0x" << ex_mem_reg.pc << ". Halting.\n"; memory_access_error = true; } else if (address > MEMORY_SIZE - access_size) { std::cerr << "Error [MEM]: Out-of-bounds Mem access Addr=0x" << std::hex << address << " Size=" << access_size << " MemSize=" << MEMORY_SIZE << " PC=0x" << ex_mem_reg.pc << ". Halting.\n"; memory_access_error = true; } if (memory_access_error) { halted = true; mem_wb_write_reg = {}; mem_wb_write_reg.raw_instruction = NOP_INSTRUCTION; mem_wb_write_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0); return; } }
    try { if (ctrl.MemRead) { int32 value = 0; for (uint32 i = 0; i < access_size; ++i) { value |= (static_cast<int32>(data_memory.at(address + i)) << (i * 8)); } mem_wb_write_reg.mem_read_data_int = value; } else if (ctrl.fpMemRead) { uint32 bits = 0; for (uint32 i = 0; i < access_size; ++i) { bits |= (static_cast<uint32>(data_memory.at(address + i)) << (i * 8)); } mem_wb_write_reg.mem_read_data_fp = bits_to_float(bits); } if (ctrl.MemWrite) { int32 value_to_store = ex_mem_reg.store_data_int; for (uint32 i = 0; i < access_size; ++i) { data_memory.at(address + i) = static_cast<uint8>((value_to_store >> (i * 8)) & 0xFF); } } else if (ctrl.fpMemWrite) { float value_fp = ex_mem_reg.store_data_fp; uint32 bits = float_to_bits(value_fp); for (uint32 i = 0; i < access_size; ++i) { data_memory.at(address + i) = static_cast<uint8>((bits >> (i * 8)) & 0xFF); } }
    } catch (const std::out_of_range& oor) { std::cerr << "Fatal Error [MEM]: Memory access std::out_of_range at address 0x" << std::hex << address << ". Halting.\n"; halted = true; mem_wb_write_reg = {}; mem_wb_write_reg.raw_instruction = NOP_INSTRUCTION; mem_wb_write_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0); }
}

void Processor::writeback() {
    if (halted) return;
    ControlSignals& ctrl = mem_wb_reg.control; uint32 rd_idx = mem_wb_reg.rd;
    if (rd_idx != 0) { if (ctrl.RegWrite) { int32 write_value = ctrl.MemToReg ? mem_wb_reg.mem_read_data_int : mem_wb_reg.alu_result; regs[rd_idx] = write_value; } else if (ctrl.fpRegWrite) { float write_value_fp = ctrl.fpMemToReg ? mem_wb_reg.mem_read_data_fp : mem_wb_reg.fp_alu_result; fregs[rd_idx] = write_value_fp; } }
    regs[0] = 0;
    if (mem_wb_reg.raw_instruction == HALT_INSTRUCTION && mem_wb_reg.raw_instruction != NOP_INSTRUCTION) { halted = true; }
}

void Processor::detect_hazards() {
    stall_if = false; flush_ifid = false; flush_idex = false;
    bool load_in_idex = (id_ex_reg.control.MemRead || id_ex_reg.control.fpMemRead); uint32 load_dest_reg = id_ex_reg.rd;
    if (load_in_idex && load_dest_reg != 0) {
        uint32 instr_in_decode = if_id_reg.instruction; uint32 decode_rs1=get_rs1(instr_in_decode); uint32 decode_rs2=get_rs2(instr_in_decode); uint32 decode_rs3=get_rs3(instr_in_decode);
        bool dependency = false; uint32 decode_opcode=get_opcode(instr_in_decode); uint32 decode_funct7=get_funct7(instr_in_decode);
        bool d_needs_irs1=false, d_needs_irs2=false, d_needs_fprs1=false, d_needs_fprs2=false, d_needs_fprs3=false;
        if(decode_opcode==0x63||decode_opcode==0x03||decode_opcode==0x23||decode_opcode==0x13||decode_opcode==0x33||decode_opcode==0x67) d_needs_irs1=true;
        if(decode_opcode==0x63||decode_opcode==0x23||decode_opcode==0x33) d_needs_irs2=true;
        if(decode_opcode==0x07||decode_opcode==0x27||decode_opcode==0x43||decode_opcode==0x47||decode_opcode==0x4B||decode_opcode==0x4F||decode_opcode==0x53) d_needs_fprs1=true;
        if(decode_opcode==0x27||decode_opcode==0x43||decode_opcode==0x47||decode_opcode==0x4B||decode_opcode==0x4F||decode_opcode==0x53) d_needs_fprs2=true;
        if(decode_opcode==0x43||decode_opcode==0x47||decode_opcode==0x4B||decode_opcode==0x4F) d_needs_fprs3=true;
        bool decode_int_to_fp = (decode_opcode==0x53) && (decode_funct7==0b1101000 || decode_funct7==0b1111000);
        bool decode_fp_to_int = (decode_opcode==0x53) && (decode_funct7==0b1100000 || decode_funct7==0b1110000 || decode_funct7==0b1010000);
        if (id_ex_reg.control.writesIntRd) { if(d_needs_irs1 && (load_dest_reg == decode_rs1)) dependency = true; if(d_needs_irs2 && (load_dest_reg == decode_rs2)) dependency = true; if(decode_int_to_fp && (load_dest_reg == decode_rs1)) dependency = true; }
        else if (id_ex_reg.control.writesFpFd) { if(d_needs_fprs1 && (load_dest_reg == decode_rs1)) dependency = true; if(d_needs_fprs2 && (load_dest_reg == decode_rs2)) dependency = true; if(d_needs_fprs3 && (load_dest_reg == decode_rs3)) dependency = true; if(decode_fp_to_int){ if (load_dest_reg == decode_rs1) dependency = true; if ((decode_funct7==0b1010000) && load_dest_reg == decode_rs2) dependency = true; } }
        if (dependency) { stall_if = true; flush_idex = true; load_stall_count++; }
    }
    if (ex_mem_reg.branch_taken) { flush_ifid = true; flush_idex = true; }
}

void Processor::tick() {
    if (halted) return;
    writeback(); memory_access(); execute(); decode(); fetch();
    detect_hazards();
    mem_wb_reg = mem_wb_write_reg;
    ex_mem_reg = ex_mem_write_reg;
    if (flush_idex) { id_ex_reg = {}; id_ex_reg.raw_instruction = NOP_INSTRUCTION; id_ex_reg.disasm_instr = disassemble(NOP_INSTRUCTION, 0); } else { id_ex_reg = id_ex_write_reg; }
    if (flush_ifid) { if_id_reg = {}; if_id_reg.instruction = NOP_INSTRUCTION; } else if (!stall_if) { if_id_reg = if_id_write_reg; }
    if (!stall_if) { if (pc_next_update != 0) { pc = pc_next_update; pc_next_update = 0; } else { pc += 4; } }
    cycle_count++;
}

bool Processor::step() {
    if (halted) return false;
    tick();
    return !halted;
}

void Processor::run_until_halt(uint64_t max_cycles) {
     std::cout << "Starting simulation (Max cycles: " << max_cycles << ")...\n";
     while (!halted) {
         if (cycle_count >= max_cycles) {
             std::cout << "\nReached max cycle limit (" << max_cycles << "). Stopping simulation.\n";
             halted = true;
             print_pipeline_state();
             break;
         }
         print_pipeline_state(); 
         if (!step()) { 
             std::cout << "\nProcessor Halted by instruction." << std::endl;
             break; 
         }
     }
     if (!halted) { 
          print_pipeline_state();
     }
     std::cout << "\nSimulation finished or halted after " << cycle_count << " cycles.\n";
     print_performance_counters();
}


// --- Debug/Visualization ---
void Processor::print_pipeline_state() {
     std::cout << "Cycle: " << std::dec << cycle_count << "\n";
     std::cout << "PC: 0x" << std::hex << pc << std::dec;
     if (pc_next_update != 0) std::cout << " (Pending Target: 0x" << std::hex << pc_next_update << std::dec << ")";
     std::cout << "\n";
     std::cout << "IF/ID : PC=0x" << std::hex << if_id_reg.pc << std::dec << " | Instr=" << disassemble(if_id_reg.instruction, if_id_reg.pc) << "\n";
     std::cout << "ID/EX : " << std::left << std::setw(18) << id_ex_reg.disasm_instr << " | rs1("<< std::setw(2) << id_ex_reg.rs1 <<")=" << std::setw(4) << id_ex_reg.read_data1 << " | rs2("<< std::setw(2) << id_ex_reg.rs2 <<")=" << std::setw(4) << id_ex_reg.read_data2 << " | rd("<< std::setw(2) << id_ex_reg.rd <<")" << " | imm=" << id_ex_reg.immediate; if (id_ex_reg.control.isFP) { std::cout << " | fs1=" << std::fixed << std::setprecision(3) << id_ex_reg.read_data_fp1 << " | fs2=" << std::fixed << std::setprecision(3) << id_ex_reg.read_data_fp2; if(id_ex_reg.control.usesFS3) std::cout << " | fs3=" << id_ex_reg.read_data_fp3; } std::cout << "\n";
     std::cout << "EX/MEM: " << std::left << std::setw(18) << ex_mem_reg.disasm_instr << " | ALURes=0x" << std::hex << ex_mem_reg.alu_result << std::dec << "(" << std::setw(4) << ex_mem_reg.alu_result << ")"; if(ex_mem_reg.control.isFP && !ex_mem_reg.control.fpResultIsInt) std::cout << " | FPRes=" << std::fixed << std::setprecision(3) << ex_mem_reg.fp_alu_result; std::cout << " | StoreInt=" << std::setw(4) << ex_mem_reg.store_data_int; if(ex_mem_reg.control.fpMemWrite) std::cout << " | StoreFP=" << std::fixed << std::setprecision(3) << ex_mem_reg.store_data_fp; std::cout << " | rd(" << std::setw(2) << ex_mem_reg.rd << ")"; if (ex_mem_reg.branch_taken) std::cout << " | Branch=Taken (Target=0x" << std::hex << ex_mem_reg.pc_branch_target << std::dec << ")"; std::cout << "\n";
     std::cout << "MEM/WB: " << std::left << std::setw(18) << mem_wb_reg.disasm_instr << " | ALURes=0x" << std::hex << mem_wb_reg.alu_result << std::dec << "(" << std::setw(4) << mem_wb_reg.alu_result << ")"; if(mem_wb_reg.control.isFP && !mem_wb_reg.control.fpResultIsInt) std::cout << " | FPRes=" << std::fixed << std::setprecision(3) << mem_wb_reg.fp_alu_result; std::cout << " | MemInt=" << std::setw(4) << mem_wb_reg.mem_read_data_int; if(mem_wb_reg.control.fpMemRead) std::cout << " | MemFP=" << std::fixed << std::setprecision(3) << mem_wb_reg.mem_read_data_fp; std::cout << " | rd(" << std::setw(2) << mem_wb_reg.rd << ")\n";
     std::cout << "Regs  : x1(ra)=" << std::setw(4) << regs[1] << " x2(sp)=" << std::setw(6) << regs[2] << " x3(gp)=" << std::setw(4) << regs[3] << " x4(tp)=" << std::setw(4) << regs[4] << " x5(t0)=" << std::setw(4) << regs[5] << " x6(t1)=" << std::setw(4) << regs[6] << " x10(a0)=" << std::setw(4) << regs[10] << " x11(a1)=" << std::setw(4) << regs[11] << "\n";
     std::cout << "FRegs : f6(ft6)=" << std::fixed << std::setprecision(3) << fregs[6] << " f7(ft7)=" << fregs[7] << " f8(fs0)=" << fregs[8] << " f9(fs1)=" << fregs[9] << " f10(fa0)=" << fregs[10] << " f11(fa1)="<<fregs[11]<<" f12(fa2)="<<fregs[12] << "\n";
     std::cout << "Ctrl ->: "; bool any_signal=false; if(stall_if){std::cout<<"STALL-IF "; any_signal=true;} if(flush_ifid){std::cout<<"FLUSH-IFID "; any_signal=true;} if(flush_idex){std::cout<<"FLUSH-IDEX "; any_signal=true;} if(!any_signal){std::cout<<"None";} std::cout<<"\n";
     std::cout << "--------------------------------------------------------------------------------\n";
 }
void Processor::dump_memory(uint32 start_addr, uint32 words_to_dump) { std::cout << "--- Data Memory Dump (Addr: 0x" << std::hex << start_addr << " - 0x" << (start_addr + words_to_dump*4 - 4) << ") ---\n"; uint32 end_addr = start_addr + words_to_dump * 4; if (start_addr >= MEMORY_SIZE) { std::cout << "Start address out of bounds.\n"; return; } if (end_addr > MEMORY_SIZE || end_addr < start_addr) end_addr = MEMORY_SIZE; for (uint32 addr = start_addr; addr < end_addr; addr += 4) { if (addr > MEMORY_SIZE - 4) break; int32 value = 0; try { for (int i = 0; i < 4; ++i) { value |= (static_cast<int32>(data_memory.at(addr + i)) << (i * 8)); } std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << addr << ": 0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32>(value) << std::dec << " (" << value << ")\n"; } catch (const std::out_of_range& oor) { std::cerr << "Error during memory dump at address 0x" << std::hex << addr << std::dec << ": " << oor.what() << std::endl; break; } } std::cout << "--- End Memory Dump ---\n"; }
void Processor::dump_registers() { std::cout << "--- Integer Register Dump ---\n"; std::cout << std::setfill(' '); for (int i = 0; i < 32; ++i) { std::cout << std::left << std::setw(3) << REG_NAMES[i] << " (x" << std::setw(2) << i << "): " << std::right << std::setw(11) << regs[i] << " (0x" << std::hex << std::setw(8) << std::setfill('0') << regs[i] << std::dec << std::setfill(' ') << ")"; if ((i + 1) % 4 == 0) std::cout << "\n"; else std::cout << " | "; } std::cout << "--- End Register Dump ---\n"; }
void Processor::dump_fregisters() { std::cout << "--- FP Register Dump ---\n"; std::cout << std::setfill(' '); for (int i = 0; i < 32; ++i) { std::cout << std::left << std::setw(4) << FREG_NAMES[i] << " (f" << std::setw(2) << i << "): " << std::right << std::fixed << std::setprecision(6) << std::setw(14) << fregs[i] << " (0x" << std::hex << std::setw(8) << std::setfill('0') << float_to_bits(fregs[i]) << std::dec << std::setfill(' ') << ")"; if ((i + 1) % 2 == 0) std::cout << "\n"; else std::cout << " | "; } std::cout << "--- End FP Register Dump ---\n"; }
void Processor::print_performance_counters() { std::cout << "--- Performance Counters ---\n"; std::cout << "Total Cycles Executed:    " << cycle_count << "\n"; std::cout << "Total Branches & Jumps:   " << branch_count << "\n"; std::cout << "Branches/Jumps Taken:     " << branch_taken << "\n"; std::cout << "Load-Use Hazard Stalls:   " << load_stall_count << "\n"; std::cout << "Control Hazard Flushes:   " << control_hazard_count << "\n"; if (branch_count > 0) { double rate = static_cast<double>(control_hazard_count) / branch_count * 100.0; std::cout << "Branch Misprediction Rate: " << std::fixed << std::setprecision(2) << rate << "%"; std::cout << " (Note: Based on simple flush-on-taken scheme)\n"; } else { std::cout << "Branch Misprediction Rate: N/A\n"; } std::cout << "--- End Performance Counters ---\n"; }
bool Processor::is_halted() const { return halted; }
uint64_t Processor::get_cycle_count() const { return cycle_count; }
size_t Processor::get_memory_size() const { return data_memory.size(); }
size_t Processor::get_instruction_memory_size() const { return instruction_memory.size(); }

int main() {
    Processor processor;
    std::vector<uint32_t> program = {
    // Basic Arithmetic and Register Dependencies
    0x00500093, 
    0x00100113, 
    0x002081b3,
    0x00300213,
    0x00418233,  
    
    // Load-Use Hazard Test (critical test case)
    0x00002283, 
    0x00128293, 
    
    // Store after Load (testing forwarding to store)
    0x0050a023,
    
    0x00100293, 
    0x00128313,  
    0x00130393,  
    0x00138413,  
    
    // Branch Not Taken
    0x00200293,
    0x00300313,  
    0x00628463, 
    0x00100393, 
    
    // Branch Taken (Control Hazard)
    0x00400413,
    0x00800493, 
    0x00940463, 
    0x00100513, 
    0x00400513, 
    0x00800593, 
    0x00B50463, 
    0x00900613, 
    
    // JAL test (Control Hazard)
    0x008000EF,  
    0xFF000693,  
    
    // Target of JAL
    0x00F00713,  
    
    // JALR test (Control Hazard)
    0x00000793, 
    0x07C78067,  
    0xFF000813, 
    0xFF000893,  
    
    // Target of JALR
    0x01000893,  
    
    // Test ADD/SUB
    0x00300F13, 
    0x00500F93,  
    0x01FF0FB3,  
    0x41FF0F33,  
    
    // Test AND/OR/XOR
    0x0FF00813, 
    0x0F000893,  
    0x0108F8B3, 
    0x0108E833,  
    0x01184833, 
    
    // Test shifts
    0x00400513, 
    0x00151513,  
    0x00255513, 
    0x40155513,  
    
    // Test SLT/SLTU
    0xFFF00A93, 
    0x00100B13,  
    0x016AAB33, 
    0x016ABB33, 
    
    // Test Load after Store (checking memory operations)
    0x01000C13,  
    0x018A2023,  
    0x000A2C83, 
    
    // Test LUI/AUIPC
    0x12345DB7, 
    0x00001D17, 
    
    // halt
    0x00000013  
};
    processor.load_program(program);

    processor.run_until_halt(100);

    std::cout << "\n--- Final State ---" << std::endl;
    processor.dump_registers();
    processor.dump_fregisters();
    processor.dump_memory(0, 24);

    return 0;
}