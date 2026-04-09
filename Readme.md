# RISC-V Pipelined Processor Simulator (RV32IMFQ)

## Overview
This project implements a cycle-accurate simulation of a 5-stage pipelined RISC-V processor in C++. It supports the RV32IMFQ instruction set, including integer, memory, and floating-point operations.

The simulator models real processor behavior such as pipeline stages, hazard detection, forwarding, stalls, and branch handling, with cycle-by-cycle output for analysis.

---

## Features

- 5-stage pipeline: Fetch, Decode, Execute, Memory, Writeback  
- Support for RV32I, M, and F extensions  
- Hazard detection and data forwarding  
- Pipeline stalls and control hazard handling  
- Floating-point operations (add, sub, mul, div, FMA, conversions)  
- Instruction decoding and disassembly  
- Cycle-by-cycle pipeline state output  
- Register and memory inspection  

---

## Project Structure
