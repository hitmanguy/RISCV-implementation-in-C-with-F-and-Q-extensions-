#ifndef RISCV_UTILS_H
#define RISCV_UTILS_H

#include "riscv_constants.h"

int32 sign_extend(uint32 value, int bits);
int32 sign_extend_12(uint32 immediate);
int32 sign_extend_b(uint32 reconstructed_imm);
int32 sign_extend_j(uint32 reconstructed_imm);

uint32 float_to_bits(float f);
float bits_to_float(uint32 bits);

std::string to_binary(uint32 n, int bits = 32);

uint32 get_opcode(uint32 instruction);
uint32 get_rd(uint32 instruction);
uint32 get_funct3(uint32 instruction);
uint32 get_rs1(uint32 instruction);
uint32 get_rs2(uint32 instruction);
uint32 get_rs3(uint32 instruction);
uint32 get_funct7(uint32 instruction);
uint32 get_funct5(uint32 instruction);

int32 get_imm_i(uint32 instruction);
int32 get_imm_s(uint32 instruction);
int32 get_imm_b(uint32 instruction);
int32 get_imm_u(uint32 instruction);
int32 get_imm_j(uint32 instruction);
uint32 get_shamt_i(uint32 instruction);

std::string disassemble(uint32 instruction, uint32 pc);

#endif 