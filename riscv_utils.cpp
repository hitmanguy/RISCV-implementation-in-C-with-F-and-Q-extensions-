#include "riscv_utils.h"
#include <iomanip>
#include <sstream>
#include <bitset>
#include <cstring>
#include <cmath>
#include <limits>

const std::array<std::string, 32> REG_NAMES = {
    "x0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};
const std::array<std::string, 32> FREG_NAMES = {
    "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
    "fs0", "fs1", "fa0", "fa1", "fa2", "fa3", "fa4", "fa5",
    "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
    "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"
};

int32 sign_extend(uint32 val, int bits) {
    if (bits <= 0 || bits >= 32) return static_cast<int32>(val);
    uint32 sign_bit_mask = 1U << (bits - 1);
    if (val & sign_bit_mask) {
        uint32 extension_mask = (0xFFFFFFFFU << bits);
        return static_cast<int32>(val | extension_mask);
    } else {
        return static_cast<int32>(val);
    }
}
int32 sign_extend_12(uint32 immediate) { return sign_extend(immediate, 12); }
int32 sign_extend_b(uint32 reconstructed_imm) { return sign_extend(reconstructed_imm, 13); }
int32 sign_extend_j(uint32 reconstructed_imm) { return sign_extend(reconstructed_imm, 21); }

uint32 float_to_bits(float f) {
    uint32 bits;
    static_assert(sizeof(float) == sizeof(uint32), "Float size mismatch");
    std::memcpy(&bits, &f, sizeof(float));
    return bits;
}
float bits_to_float(uint32 bits) {
    float f;
    static_assert(sizeof(float) == sizeof(uint32), "Float size mismatch");
    std::memcpy(&f, &bits, sizeof(uint32));
    return f;
}

std::string to_binary(uint32 n, int bits) {
    if (bits <= 0 || bits > 32) return "Invalid bit size";
    return std::bitset<32>(n).to_string().substr(32 - bits);
}

uint32 get_opcode(uint32 instr) { return instr & 0x7F; }
uint32 get_rd(uint32 instr) { return (instr >> 7) & 0x1F; }
uint32 get_funct3(uint32 instr) { return (instr >> 12) & 0x7; }
uint32 get_rs1(uint32 instr) { return (instr >> 15) & 0x1F; }
uint32 get_rs2(uint32 instr) { return (instr >> 20) & 0x1F; }
uint32 get_rs3(uint32 instr) { return (instr >> 27) & 0x1F; }
uint32 get_funct7(uint32 instr) { return (instr >> 25) & 0x7F; }
uint32 get_funct5(uint32 instr) { return (instr >> 27) & 0x1F; }

int32 get_imm_i(uint32 instr) { return sign_extend_12(instr >> 20); }
int32 get_imm_s(uint32 instr) {
    uint32 imm_11_5 = get_funct7(instr); uint32 imm_4_0 = get_rd(instr);
    return sign_extend_12((imm_11_5 << 5) | imm_4_0);
}
int32 get_imm_b(uint32 instr) {
    uint32 imm_12 = (instr >> 31) & 1; uint32 imm_10_5 = (instr >> 25) & 0x3F;
    uint32 imm_4_1 = (instr >> 8) & 0xF; uint32 imm_11 = (instr >> 7) & 1;
    uint32 combined = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
    return sign_extend_b(combined);
}
int32 get_imm_u(uint32 instr) { return static_cast<int32>(instr & 0xFFFFF000); }
int32 get_imm_j(uint32 instr) {
    uint32 imm_20 = (instr >> 31) & 1; uint32 imm_10_1 = (instr >> 21) & 0x3FF;
    uint32 imm_11 = (instr >> 20) & 1; uint32 imm_19_12 = (instr >> 12) & 0xFF;
    uint32 combined = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
    return sign_extend_j(combined);
}
uint32 get_shamt_i(uint32 instr) { return (instr >> 20) & 0x1F; }

std::string disassemble(uint32 instr, uint32 pc) {
     if (instr == NOP_INSTRUCTION) return "nop";
     if (instr == HALT_INSTRUCTION) return "<halt>";

     uint32 opcode = get_opcode(instr); uint32 rd_idx = get_rd(instr);
     uint32 rs1_idx = get_rs1(instr); uint32 rs2_idx = get_rs2(instr);
     uint32 rs3_idx = get_rs3(instr); uint32 funct3 = get_funct3(instr);
     uint32 funct7 = get_funct7(instr);

     std::stringstream ss;
     std::string rd_str = (rd_idx < 32) ? REG_NAMES[rd_idx] : "inv";
     std::string rs1_str = (rs1_idx < 32) ? REG_NAMES[rs1_idx] : "inv";
     std::string rs2_str = (rs2_idx < 32) ? REG_NAMES[rs2_idx] : "inv";
     std::string fd_str = (rd_idx < 32) ? FREG_NAMES[rd_idx] : "inv";
     std::string fs1_str = (rs1_idx < 32) ? FREG_NAMES[rs1_idx] : "inv";
     std::string fs2_str = (rs2_idx < 32) ? FREG_NAMES[rs2_idx] : "inv";
     std::string fs3_str = (rs3_idx < 32) ? FREG_NAMES[rs3_idx] : "inv";

     ss << std::left << std::setw(8);

    switch (opcode) {
        case 0x37: ss << "lui" << rd_str << ", 0x" << std::hex << (get_imm_u(instr) >> 12); break;
        case 0x17: ss << "auipc" << rd_str << ", 0x" << std::hex << (get_imm_u(instr) >> 12); break;
        case 0x6F: ss << "jal" << rd_str << ", 0x" << std::hex << (pc + get_imm_j(instr)); break;
        case 0x67: if (funct3 == 0) ss << "jalr" << rd_str << ", " << rs1_str << ", " << std::dec << get_imm_i(instr); else ss << "inv_jalr"; break;
        case 0x63: { std::string m; switch (funct3) { case 0: m="beq"; break; case 1: m="bne"; break; case 4: m="blt"; break; case 5: m="bge"; break; case 6: m="bltu"; break; case 7: m="bgeu"; break; default: m="inv_br"; break; } ss << m << " " << rs1_str << ", " << rs2_str << ", 0x" << std::hex << (pc + get_imm_b(instr)); break; }
        case 0x03: { std::string m="inv_ld"; switch (funct3) { case 0x2: m = "lw"; break; } ss << m << rd_str << ", " << std::dec << get_imm_i(instr) << "(" << rs1_str << ")"; break; }
        case 0x23: { std::string m="inv_st"; switch (funct3) { case 0x2: m = "sw"; break; } ss << m << rs2_str << ", " << std::dec << get_imm_s(instr) << "(" << rs1_str << ")"; break; }
        case 0x13: { std::string m; int32 imm=get_imm_i(instr); uint32 sh=get_shamt_i(instr); switch (funct3) { case 0: m="addi"; break; case 1: m="slli"; imm=sh; break; case 2: m="slti"; break; case 3: m="sltiu"; break; case 4: m="xori"; break; case 5: if(((instr>>30)&1)==0) m="srli"; else m="srai"; imm=sh; break; case 6: m="ori"; break; case 7: m="andi"; break; default: m="inv_imm"; break; } ss << m << " " << rd_str << ", " << rs1_str << ", " << std::dec << imm; break; }
        case 0x33: { std::string m; bool is_m=(funct7==0x01); if (!is_m) { bool alt=(funct7==0x20); switch (funct3) { case 0: m=alt?"sub":"add"; break; case 1: m="sll"; break; case 2: m="slt"; break; case 3: m="sltu"; break; case 4: m="xor"; break; case 5: m=alt?"sra":"srl"; break; case 6: m="or"; break; case 7: m="and"; break; default: m="inv_r"; break; } } else { switch(funct3){ case 0:m="mul"; break; case 1:m="mulh"; break; case 2:m="mulhsu"; break; case 3:m="mulhu"; break; case 4:m="div"; break; case 5:m="divu"; break; case 6:m="rem"; break; case 7:m="remu"; break; default: m="inv_m"; break; } } ss << m << " " << rd_str << ", " << rs1_str << ", " << rs2_str; break; }
        case 0x07: if (funct3 == 0x2) ss << "flw" << fd_str << ", " << std::dec << get_imm_i(instr) << "(" << rs1_str << ")"; else ss << "inv_flw"; break;
        case 0x27: if (funct3 == 0x2) ss << "fsw" << fs2_str << ", " << std::dec << get_imm_s(instr) << "(" << rs1_str << ")"; else ss << "inv_fsw"; break;
        case 0x43: ss << "fmadd.s" << fd_str << ", " << fs1_str << ", " << fs2_str << ", " << fs3_str; break;
        case 0x47: ss << "fmsub.s" << fd_str << ", " << fs1_str << ", " << fs2_str << ", " << fs3_str; break;
        case 0x4B: ss << "fnmsub.s"<< fd_str << ", " << fs1_str << ", " << fs2_str << ", " << fs3_str; break;
        case 0x4F: ss << "fnmadd.s"<< fd_str << ", " << fs1_str << ", " << fs2_str << ", " << fs3_str; break;
         case 0x53: { std::string m = "inv_fp"; switch (funct7) { case 0b0000000: m="fadd.s"; break; case 0b0000100: m="fsub.s"; break; case 0b0001000: m="fmul.s"; break; case 0b0001100: m="fdiv.s"; break; case 0b0101100: if(rs2_idx==0) m="fsqrt.s"; else m="inv_sqrt"; break; case 0b0010000: switch(funct3){ case 0: m="fsgnj.s"; break; case 1: m="fsgnjn.s"; break; case 2: m="fsgnjx.s"; break; default:m="inv_sgn"; break; } break; case 0b0010100: switch(funct3){ case 0: m="fmin.s"; break; case 1: m="fmax.s"; break; default:m="inv_minmax"; break; } break; case 0b1100000: if(rs2_idx==0) m="fcvt.w.s"; else if(rs2_idx==1) m="fcvt.wu.s"; else m="inv_fcvt.w"; break; case 0b1110000: if(funct3==0) m="fmv.x.w"; else if(funct3==1) m="fclass.s"; else m="inv_fmvx_fclass"; break; case 0b1010000: switch(funct3){ case 0: m="flt.s"; break; case 1: m="fle.s"; break; case 2: m="feq.s"; break; default:m="inv_fcmp"; break; } break; case 0b1101000: if(rs2_idx==0) m="fcvt.s.w"; else if(rs2_idx==1) m="fcvt.s.wu"; else m="inv_fcvt.s"; break; case 0b1111000: if(funct3==0) m="fmv.w.x"; else m="inv_fmvw"; break; default: m="inv_fp_f7"; break;}
             if (m.rfind("inv", 0) == 0) { ss << m; } else if (m.find("fcvt.w")!=std::string::npos || m.find("fmv.x")!=std::string::npos || m.find("fclass")!=std::string::npos || m.find("fcmp")!=std::string::npos) { ss << m << " " << rd_str << ", " << fs1_str; if(m.find("fcmp")!=std::string::npos) ss << ", " << fs2_str; } else if (m.find("fcvt.s")!=std::string::npos || m.find("fmv.w")!=std::string::npos) { ss << m << " " << fd_str << ", " << rs1_str; } else if (m.find("fsqrt")!=std::string::npos) { ss << m << " " << fd_str << ", " << fs1_str; } else { ss << m << " " << fd_str << ", " << fs1_str << ", " << fs2_str; } break; }
        default: ss << "unknown_op"; break;
    }

     std::string result = ss.str();
     size_t endpos = result.find_last_not_of(" \t");
     if (std::string::npos != endpos) result = result.substr(0, endpos + 1); else result.clear();
     const size_t min_disasm_len = 18;
     if (result.length() < min_disasm_len) result += std::string(min_disasm_len - result.length(), ' ');
     return result;
}