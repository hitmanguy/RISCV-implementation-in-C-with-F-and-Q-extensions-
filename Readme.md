# RISC-V Pipelined Processor Simulator

## Overview
This project is a cycle-accurate RISC-V pipeline simulator written in C++. It models a classic 5-stage pipeline and prints detailed execution traces so you can observe how instructions move through the processor cycle by cycle.

The simulator demonstrates core processor concepts such as instruction fetch/decode, data hazards, load-use stalls, forwarding, branch/jump handling, register updates, and final architectural state dumps.

## Features
- 5-stage pipeline: `IF`, `ID`, `EX`, `MEM`, `WB`
- Cycle-by-cycle pipeline state tracing
- Integer instruction support for common RV32I operations
- Support for multiply/divide-style decoding from the `M` extension
- Support for single-precision floating-point decode/utilities in the `F` extension
- Hazard detection for load-use dependencies
- Data forwarding between pipeline stages
- Control hazard handling for branches, `jal`, and `jalr`
- Performance counters for cycles, stalls, flushes, and taken branches
- Final dumps for integer registers, floating-point registers, and memory

## Current Demo Program
The sample program embedded in `main()` exercises:
- arithmetic and register dependencies
- load/store behavior
- a load-use hazard stall
- branch taken and not-taken cases
- `jal` and `jalr` control transfers
- shifts, logic instructions, comparisons, `lui`, and `auipc`

## Project Structure

```text
processor.cpp        Core simulator implementation and demo program
processor.h          Processor class interface and counters
pipeline_defs.h      Pipeline register/control structure definitions
riscv_utils.cpp      Instruction decoding, disassembly, and bit utilities
riscv_utils.h        Utility declarations
riscv_constants.h    Constants, typedefs, and instruction values
```

## Requirements
- `g++` or another C++ compiler with C++11 support
- Windows, Linux, or WSL terminal

## Build

```bash
g++ -std=c++11 processor.cpp riscv_utils.cpp -o simulator
```

## Run

```bash
./simulator
```

On Windows with MinGW:

```bash
simulator.exe
```

## Sample Output
Below is a shortened sample from the simulator output showing normal flow, a load-use stall, and control hazard flushing:

```text
Starting simulation (Max cycles: 100)...

Cycle: 3
PC: 0xc
IF/ID : PC=0x8 | Instr=add      gp, ra, sp
ID/EX : addi     sp, x0, 1 | rs1(0 )=0 | rs2(1 )=0 | rd(2 ) | imm=1
EX/MEM: addi     ra, x0, 5 | ALURes=0x5(5) | StoreInt=0 | rd(1 )
MEM/WB: nop
Ctrl ->: None

Cycle: 8
PC: 0x1c
IF/ID : PC=0x18 | Instr=addi     t0, t0, 1
ID/EX : nop                | rs1(0 )=0 | rs2(0 )=0 | rd(0 ) | imm=0
EX/MEM: lw      t0, 0(x0)  | ALURes=0x0(0) | StoreInt=0 | rd(5 )
MEM/WB: add      tp, gp, tp | ALURes=0x9(9) | MemInt=0 | rd(4 )
Ctrl ->: STALL-IF FLUSH-IDEX

Cycle: 28
PC: 0x6c
IF/ID : PC=0x68 | Instr=addi     a4, x0, 15
ID/EX : addi     a3, x0, -16 | rs1(0 )=0 | rs2(16)=0 | rd(13) | imm=-16
EX/MEM: jal     ra, 0x68   | ALURes=0x64(100) | StoreInt=4 | rd(1 ) | Branch=Taken (Target=0x68)
MEM/WB: addi     a2, x0, 9 | ALURes=0x9(9) | MemInt=0 | rd(12)
Ctrl ->: None

Cycle: 29
PC: 0x68
IF/ID : PC=0x0 | Instr=nop
ID/EX : nop                | rs1(0 )=0 | rs2(0 )=0 | rd(0 ) | imm=0
EX/MEM: addi     a3, x0, -16 | ALURes=0xfffffff0(-16) | StoreInt=0 | rd(13)
MEM/WB: jal     ra, 0x68   | ALURes=0x64(100) | MemInt=0 | rd(1 )
Ctrl ->: FLUSH-IFID FLUSH-IDEX

Processor Halted by instruction.

Simulation finished or halted after 64 cycles.
--- Performance Counters ---
Total Cycles Executed:    64
Total Branches & Jumps:   5
Branches/Jumps Taken:     2
Load-Use Hazard Stalls:   1
Control Hazard Flushes:   2
Branch Misprediction Rate: 40.00% (Note: Based on simple flush-on-taken scheme)
```

## Example Final State
At the end of the bundled demo run:
- `ra = 100`
- `gp = 6`
- `tp = 9`
- `t5 = -2`
- `t6 = 8`
- memory at `0x00000000 = 16`
- memory at `0x00000004 = 170`
- memory at `0x00000008 = 187`

## Notes
- The demo program is currently hardcoded in [`processor.cpp`](/d:/hobby/RISCV-implementation-in-C-with-F-and-Q-extensions-/processor.cpp).
- The current run mainly demonstrates integer pipeline behavior; floating-point support exists in the decoder/utilities and register dump path.
- The simulator stops when the halt instruction is encountered and then prints performance counters plus final state.

## Author
Sahil Chauhan
