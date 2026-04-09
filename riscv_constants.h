
#ifndef RISCV_CONSTANTS_H
#define RISCV_CONSTANTS_H

#include <cstdint>
#include <string>  
#include <array>   

using int32 = int32_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;
using uint8 = uint8_t;

#ifdef __GNUC__
using float128 = __float128;
#else
struct float128 { uint64 low; uint64 high; };
#endif


const uint32 MEMORY_SIZE = 64 * 1024;   
const uint32 INSTR_MEMORY_SIZE = 1024;     


const uint32 NOP_INSTRUCTION = 0x00000013;
const uint32 HALT_INSTRUCTION = 0x00000000;

extern const std::array<std::string, 32> REG_NAMES;

extern const std::array<std::string, 32> FREG_NAMES;

#endif 