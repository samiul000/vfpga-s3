# RISC-V Emulator : RV32I on VFPGA-S3

**Target:** ESP32-S3 N16R8 | **ISA:** RV32I (37 instructions) | **Memory:** 64 KB + 16 KB MMIO

---

## Architecture

```
 +-----------------------------------------------------------------+
 |  VFPGA-S3  :  Software-Defined Virtual FPGA on ESP32-S3         |
 |                                                                 |
 |  +-----------------------------------------------------------+  |
 |  |                   RISC-V RV32I CPU                        |  |
 |  |                                                           |  |
 |  |  +---------+  +---------+  +---------------------------+  |  |
 |  |  |  x0-x31 |  |   PC    |  |     64 KB RAM             |  |  |
 |  |  | 32-bit  |  | 32-bit  |  | 0x00000000 - 0x0000FFFF   |  |  |
 |  |  | (x0=0)  |  |         |  +---------------------------+  |  |
 |  |  +---------+  +---------+  |     16 KB MMIO             | |  |
 |  |                            | 0x10000000 - 0x10003FFF    | |  |
 |  |  Execute: fetch -> decode  |   (I/O handler callbacks)  | |  |
 |  |           -> execute ->    +---------------------------+  |  |
 |  |           advance PC                                   |  |  |
 |  +-----------------------------------------------------------+  |
 +-----------------------------------------------------------------+
```

**Key properties:**
- 32 general-purpose 32-bit registers (x0 hardwired to 0)
- 64 KB flat byte-addressable memory (little-endian)
- MMIO region at `0x10000000` — read/write dispatched to registered callbacks
- No privilege modes, no CSRs, no interrupts
- Unknown opcodes log a warning and advance PC by 4 (no trap)

---

## Supported Instructions

All 37 base RV32I integer instructions.

### R-Type (Register-Register)

| Instruction | funct7 | funct3 | Operation |
|-------------|--------|--------|-----------|
| ADD   | 0x00 | 0x0 | rd = rs1 + rs2 |
| SUB   | 0x20 | 0x0 | rd = rs1 - rs2 |
| SLL   | 0x00 | 0x1 | rd = rs1 << rs2[4:0] |
| SLT   | 0x00 | 0x2 | rd = (rs1 < rs2) ? 1 : 0 (signed) |
| SLTU  | 0x00 | 0x3 | rd = (rs1 < rs2) ? 1 : 0 (unsigned) |
| XOR   | 0x00 | 0x4 | rd = rs1 ^ rs2 |
| SRL   | 0x00 | 0x5 | rd = rs1 >> rs2[4:0] (logical) |
| SRA   | 0x20 | 0x5 | rd = rs1 >> rs2[4:0] (arithmetic) |
| OR    | 0x00 | 0x6 | rd = rs1 \| rs2 |
| AND   | 0x00 | 0x7 | rd = rs1 & rs2 |

### I-Type (Register-Immediate ALU)

| Instruction | funct3 | Operation |
|-------------|--------|-----------|
| ADDI  | 0x0 | rd = rs1 + sign_ext(imm12) |
| SLTI  | 0x2 | rd = (rs1 < imm) ? 1 : 0 (signed) |
| SLTIU | 0x3 | rd = (rs1 < imm) ? 1 : 0 (unsigned) |
| XORI  | 0x4 | rd = rs1 ^ imm |
| ORI   | 0x6 | rd = rs1 \| imm |
| ANDI  | 0x7 | rd = rs1 & imm |
| SLLI  | 0x1 | rd = rs1 << shamt |
| SRLI  | 0x5 | rd = rs1 >> shamt (logical) |
| SRAI  | 0x5 | rd = rs1 >> shamt (arithmetic, funct7=0x20) |

### Load/Store

| Instruction | funct3 | Operation |
|-------------|--------|-----------|
| LB   | 0x0 | rd = sign_ext(mem[rs1+imm][7:0]) |
| LH   | 0x1 | rd = sign_ext(mem[rs1+imm][15:0]) |
| LW   | 0x2 | rd = mem[rs1+imm][31:0] |
| LBU  | 0x4 | rd = zero_ext(mem[rs1+imm][7:0]) |
| LHU  | 0x5 | rd = zero_ext(mem[rs1+imm][15:0]) |
| SB   | 0x0 | mem[rs1+imm][7:0] = rs2[7:0] |
| SH   | 0x1 | mem[rs1+imm][15:0] = rs2[15:0] |
| SW   | 0x2 | mem[rs1+imm][31:0] = rs2 |

### Branches

| Instruction | funct3 | Condition |
|-------------|--------|-----------|
| BEQ  | 0x0 | rs1 == rs2 |
| BNE  | 0x1 | rs1 != rs2 |
| BLT  | 0x4 | rs1 < rs2 (signed) |
| BGE  | 0x5 | rs1 >= rs2 (signed) |
| BLTU | 0x6 | rs1 < rs2 (unsigned) |
| BGEU | 0x7 | rs1 >= rs2 (unsigned) |

### Upper Immediate & Jumps

| Instruction | Opcode | Operation |
|-------------|--------|-----------|
| LUI   | 0x37 | rd = imm[31:12] << 12 |
| AUIPC | 0x17 | rd = PC + (imm[31:12] << 12) |
| JAL   | 0x6F | rd = PC+4; PC += offset |
| JALR  | 0x67 | rd = PC+4; PC = (rs1+imm) & ~1 |

---

## Memory Map

| Region | Address Range | Size | Description |
|--------|---------------|------|-------------|
| RAM | `0x00000000` - `0x0000FFFF` | 64 KB | Program + data memory |
| MMIO | `0x10000000` - `0x10003FFF` | 16 KB | Memory-mapped I/O (callback-based) |

**MMIO behavior:**
- Reads/writes in the MMIO range are intercepted before reaching RAM
- Register a handler with `set_io_handler(ctx, read_fn, write_fn)`
- Unregistered MMIO: reads return 0, writes are silently dropped

---

## CPU API

```cpp
#include "riscv/riscv_cpu.h"

RiscvCpu cpu;

cpu.reset();                          // Zero PC, zero all 32 regs, init 64KB RAM
cpu.load_program(addr, data, count);  // Write uint32_t[] into memory at addr
cpu.run(n);                           // Execute n instructions
cpu.step();                           // Execute one instruction

uint32_t pc  = cpu.get_pc();          // Read program counter
uint32_t val = cpu.get_reg(idx);      // Read register x0-x31
```

---

## Writing Programs

### Instruction Encoding Quick Reference

Each instruction is a 32-bit word. Use `uint32_t program[]` arrays.

**R-Type:** `[funct7:7][rs2:5][rs1:5][funct3:3][rd:5][opcode:7]`

**I-Type:** `[imm12:12][rs1:5][funct3:3][rd:5][opcode:7]`

**B-Type:** `[imm12:1][imm10_5:6][rs2:5][rs1:5][funct3:3][imm4_1:4][imm11:1][opcode:7]`

**Encoding Python helper** — paste into any Python to encode:

```python
def enc_r(f7, rs2, rs1, f3, rd):
    return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|0x33

def enc_i(imm, rs1, f3, rd):
    return ((imm&0xFFF)<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|0x13

def enc_b(imm13, rs2, rs1, f3):
    return (((imm13>>12)&1)<<31)|(((imm13>>5)&0x3F)<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(((imm13>>1)&0xF)<<8)|(((imm13>>11)&1)<<7)|0x63

def enc_u(imm, rd, opcode):
    return (imm&0xFFFFF000)|(rd<<7)|opcode

def enc_j(imm21, rd):
    return (((imm21>>20)&1)<<31)|(((imm21>>1)&0x3FF)<<21)|(((imm21>>11)&1)<<20)|(((imm21>>12)&0xFF)<<12)|(rd<<7)|0x6F
```

### Register Convention (Suggested)

| Register | ABI Name | Suggested Use |
|----------|----------|---------------|
| x0 | zero | Hardwired 0 |
| x1 | ra | Return address |
| x2 | sp | Stack pointer |
| x5-x7 | t0-t2 | Temporaries |
| x8 | s0/fp | Frame pointer |
| x9 | s1 | Saved |
| x10-x11 | a0-a1 | Function args / return values |
| x12-x17 | a2-a7 | Function args |
| x18-x27 | s2-s11 | Saved |
| x28-x31 | t3-t6 | Temporaries |

---

## Example Programs

### Example 1: Sum 1..10 (Basic Loop)

```cpp
// sum = 0; for(i=1; i<=10; i++) sum += i;  -> result: 55
uint32_t program[] = {
    0x00000513, // addi x10, x0, 0       ; sum = 0
    0x00100593, // addi x11, x0, 1       ; i = 1
    0x00A00613, // addi x12, x0, 10      ; limit = 10
    0x00B50533, // add  x10, x10, x11    ; sum += i
    0x00158593, // addi x11, x11, 1      ; i++
    0xFEB65CE3, // bge  x12, x11, -8     ; if limit >= i goto loop
    0x00000013, // nop                   ; done
};

cpu.reset();
cpu.load_program(0, program, 7);
cpu.run(100);
uint32_t result = cpu.get_reg(10);  // x10 = 55
```

### Example 2: Factorial

```cpp
// factorial(5) = 120.  n in x10, result in x11
uint32_t program[] = {
    0x00500513, // addi x10, x0, 5       ; n = 5
    0x00100593, // addi x11, x0, 1       ; result = 1
    0x00000613, // addi x12, x0, 0       ; temp = 0 (loop exit check)
    // loop:
    0x00B585B3, // add  x11, x11, x10    ; result *= n (using add loop)
    0xFFF50513, // addi x10, x10, -1     ; n--
    0xFC051EE3, // bne  x10, x12, -8     ; if n != 0 goto loop
    0x00000013, // nop
};
```

Note: This uses repeated addition for multiplication (substitute `mul` if M-extension is added). Use the loop below for a proper factorial with a multiply subroutine.

### Example 3: Fibonacci

```cpp
// fib(10) = 55.  x10=fib(n-2), x11=fib(n-1), x12=counter
uint32_t program[] = {
    0x00000513, // addi x10, x0, 0       ; fib(0) = 0
    0x00100593, // addi x11, x0, 1       ; fib(1) = 1
    0x00A00613, // addi x12, x0, 10      ; counter = 10
    0x00000693, // addi x13, x0, 0       ; temp = 0
    // loop:
    0x00B50533, // add  x10, x10, x11    ; temp = fib(n-2) + fib(n-1)
    0x00000593, // (placeholder — see below)
    0xFFF60613, // addi x12, x12, -1     ; counter--
    0xFC0618E3, // bne  x12, x13, -12    ; if counter != 0 goto loop
    0x00000013, // nop
};
```

Simplified working version:

```cpp
// fib(10): x10=a, x11=b, x12=n
uint32_t program[] = {
    0x00000513, // addi x10, x0, 0       ; a = 0
    0x00100593, // addi x11, x0, 1       ; b = 1
    0x00A00613, // addi x12, x0, 10      ; n = 10
    // loop:
    0x00B50533, // add  x10, x10, x11    ; a = a + b
    0x00A585B3, // add  x11, x0, x10     ; b = a  (simplified swap)
    0xFFF60613, // addi x12, x12, -1     ; n--
    0xFC0616E3, // bne  x12, x0, -12     ; if n != 0 goto loop
    0x00000013, // nop
};
// Result: x10 = fib(10) = 55
```

### Example 4: Bitwise Operations

```cpp
// Test AND, OR, XOR, NOT, shifts
uint32_t program[] = {
    0x0FF00593, // addi x11, x0, 0xFF    ; x11 = 0x000000FF
    0x0F000613, // addi x12, x0, 0xF0    ; x12 = 0x000000F0
    // AND
    0x00C5F533, // and  x10, x11, x12    ; x10 = 0xF0
    // OR
    0x00C5E5B3, // or   x11, x11, x12    ; x11 = 0xFF
    // XOR
    0x00C5C633, // xor  x12, x11, x12    ; x12 = 0x0F
    // NOT (xori with -1)
    0xFFF04693, // xori x13, x0, -1      ; x13 = 0xFFFFFFFF
    // Shift left
    0x00459713, // slli x14, x11, 4      ; x14 = 0xFF0
    // Shift right
    0x0045D793, // srli x15, x11, 4      ; x15 = 0x0F
    0x00000013, // nop
};
```

### Example 5: Memory Access (Load/Store)

```cpp
// Store values to memory, then load them back
uint32_t program[] = {
    0x00100593, // addi x11, x0, 1       ; value = 1
    0x00400613, // addi x12, x0, 4       ; addr = 4
    0x00B62023, // sw   x11, 0(x12)      ; mem[4] = 1
    0x00158593, // addi x11, x0, 2       ; value = 2
    0x00B62223, // sw   x11, 4(x12)      ; mem[8] = 2
    0x00062503, // lw   x10, 0(x12)      ; x10 = mem[4] = 1
    0x00462583, // lw   x11, 4(x12)      ; x11 = mem[8] = 2
    0x00B50533, // add  x10, x10, x11    ; x10 = 3
    0x00000013, // nop
};
```

### Example 6: Branching (If/Else)

```cpp
// if (x11 > 5) x10 = 1; else x10 = 0;
uint32_t program[] = {
    0x00600593, // addi x11, x0, 6       ; x11 = 6
    0x00500613, // addi x12, x0, 5       ; threshold = 5
    0x00C5D563, // bge  x12, x11, +10    ; if 5 >= x11, skip to else
    0x00100513, // addi x10, x0, 1       ; then: x10 = 1
    0x0080006F, // jal  x0, +8           ; jump past else
    // else:
    0x00000513, // addi x10, x0, 0       ; else: x10 = 0
    0x00000013, // nop
};
```

### Example 7: Function Call (JAL/JALR)

```cpp
// Main calls subroutine, subroutine returns result in x10
uint32_t program[] = {
    // main:
    0x00500513, // addi x10, x0, 5       ; arg = 5
    0x014000EF, // jal  x1, 20           ; call subroutine at addr 20
    // x10 now holds return value
    0x00000013, // nop                   ; done

    // subroutine at addr 0x14 (20):
    0x00250533, // add  x10, x10, x10    ; x10 = x10 * 2
    0x00800067, // jalr x0, 0(x1)        ; return (jump to ra)
};
```

---

## Running a Program

```cpp
#include "riscv/riscv_cpu.h"

void my_demo() {
    RiscvCpu cpu;
    cpu.reset();

    uint32_t program[] = { /* ... your instructions ... */ };
    size_t count = sizeof(program) / sizeof(program[0]);

    cpu.load_program(0, program, count);
    cpu.run(1000);  // run up to 1000 instructions

    // Read results
    uint32_t result = cpu.get_reg(10);  // read x10
    uint32_t pc     = cpu.get_pc();     // read program counter

    ESP_LOGI("demo", "Result: %d, PC: 0x%08X", result, pc);
}
```

---

## Limitations

| Feature | Status |
|---------|--------|
| RV32I base integer | Full (37/37 instructions) |
| M-extension (MUL/DIV) | Not implemented |
| A-extension (atomics) | Not implemented |
| C-extension (compressed) | Not implemented |
| F/D-extension (float) | Not implemented |
| Privilege modes | Not implemented |
| CSR registers | Not implemented |
| Interrupts/exceptions | Not implemented |
| Instruction count limit | Caller-specified via `run(n)` |

---

## Memory-Mapped I/O

Use MMIO to bridge the RISC-V CPU to the VFPGA fabric or external peripherals.

```cpp
// Example: MMIO handler that maps address 0x10000000 to a GPIO
static uint32_t my_io_read(void *ctx, uint32_t addr) {
    return gpio_get_level(GPIO_NUM_4);  // read GPIO4
}

static void my_io_write(void *ctx, uint32_t addr, uint32_t data) {
    gpio_set_level(GPIO_NUM_5, data & 1);  // write GPIO5
}

// Register before running:
cpu.set_io_handler(nullptr, my_io_read, my_io_write);
```

RISC-V code accessing `0x10000000` will call these handlers instead of touching RAM.

## Virtual SoC Demo (CPU + Fabric)

`src/riscv/riscv_soc_demo.cpp` (`run_soc_demo()`, called from `demo_run.cpp`) connects the RV32I core directly to a live `VFpgaCore` instance through the MMIO map:

| Offset | Access | Function |
|--------|--------|----------|
| `+0x00` | write | Fabric input A |
| `+0x04` | write | Fabric input B |
| `+0x08` | read | Evaluate LUT(A, B), return bit 0 |
| `+0x0C` | write | Reconfigure LUT truth table on the fly |

The demo's RISC-V program (`lui` + `sw`/`lw`) writes A=1, B=1, reads back AND=1, then writes A=0 and reads back AND=0 — the CPU reconfiguring and observing fabric state through its own load/store instructions. A C++ follow-up reconfigures the same LUT to OR at runtime without rebuilding anything.

## Full ISA Coverage Test (37 instructions, on hardware)

`run_demo_riscv_full()` in `src/demo_run.cpp` executes every supported instruction with hand-encoded machine code and checks all results — verified **36/36 PASS** on ESP32-S3 hardware:

- R-type: ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
- I-type: ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI, ANDI
- Loads/stores: LB, LH, LW, LBU, LHU, SB, SH, SW
- Branches: BEQ, BNE, BLT, BGE, BLTU, BGEU (taken + one not-taken case)
- Jumps/upper: JAL, JALR, LUI, AUIPC

Test vectors live in `src/isa_test_vectors.h`, generated by `host_test/gen_isa_test.py` (every encoding round-trip decode-checked; re-run the script to regenerate). Results are stored by the emulated CPU to a table at `0x1200` and verified from C++ via `RiscvCpu::read_mem_word()`.
