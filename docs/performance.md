# VFPGA-S3 Hardware Performance Report

**Board:** ESP32-S3-DevKitC-1 N16R8
**Framework:** ESP-IDF 6.0.1 (PlatformIO)

---

## Hardware Specifications

| Component | Value |
|-----------|-------|
| SoC | ESP32-S3 rev 2, Xtensa LX7 |
| CPU Cores | 2 @ 240 MHz |
| Internal RAM | 512 KB (335 KB usable) |
| PSRAM | 8 MB (Octal, 80 MHz) |
| Flash | 16 MB (QIO) |

---

## VFPGA Resource Capacity

| Resource | Capacity | Memory |
|----------|----------|--------|
| LUT4s | **4,096** | 48 KB |
| Flip-Flops | **4,096** | 48 KB |
| Signals/Nets | **4,096** | 16 KB |
| BRAM | 3 configurable software BRAM blocks (64/256/1024 x 32-bit) | 96 bytes |
| DSP | 4 configurable software multiply-accumulate units (INT8/16/32) | — |
| **Total** | | **400 KB PSRAM** |

**UP5K-class software-defined FPGA fabric** (4,096 LUT4s, approaching the LUT capacity of the Lattice iCE40 UP5K's 5,280 LUTs; timing, routing, DSP, and memory architectures differ fundamentally from silicon).

---

## LUT4 Scaling Benchmark

XOR chain design: `lut[i] = lut[i-1] ^ lut[i-2]`. Each `evaluate_combinational()` call processes all LUTs.

| LUT4 Count | Memory | Latency (us/eval) | Throughput (Keval/s) |
|------------|--------|-------------------|----------------------|
| 64 | 2.5 KB | **0.3** | **2,933** |
| 256 | 10 KB | **1.9** | **538** |
| 1,024 | 40 KB | **8.5** | **118** |
| 2,048 | 160 KB | **18.0** | **56** |
| 4,096 | 400 KB | **36.0** | **28** |

### Analysis

- Throughput scales linearly with LUT count: ~28 Keval/s at 4K, ~118 Keval/s at 1K
- 64 LUTs run at ~50,000 cycles/sec equivalent (0.3 us/eval)
- 4K LUTs run at ~28,000 cycles/sec equivalent (36 us/eval)
- Memory scales as `O(N)` where N = LUT count: ~100 bytes per LUT

---

## DSP Throughput

| Operation | Throughput (Mops/s) | Latency (us/op) |
|-----------|---------------------|------------------|
| INT8 Multiply | **6.1** | 0.16 |
| INT16 Multiply | **6.6** | 0.15 |
| INT32 Multiply | **8.3** | 0.12 |
| Multiply-Add (INT32) | **7.2** | 0.14 |

Peak: **8.3 Mops/s** (INT32 multiply)

---

## BRAM Throughput

| Block Size | Operation | Throughput (Mops/s) | Latency (us/op) |
|------------|-----------|---------------------|------------------|
| 64 x 32-bit | Write | **5.0** | 0.20 |
| 64 x 32-bit | Read | **5.1** | 0.20 |
| 256 x 32-bit | Write | **5.4** | 0.19 |
| 256 x 32-bit | Read | **5.1** | 0.20 |
| 1024 x 32-bit | Write | **5.4** | 0.19 |
| 1024 x 32-bit | Read | **5.1** | 0.20 |

Consistent ~5 Mops/s across all block sizes.

---

## Boot & Test Timing

| Phase | Time (ms) |
|-------|-----------|
| Boot + Init | 855 |
| Self-Tests (M0) | 500 |
| Bit-Parallel (M1) | 3 |
| LUT4 (M2) | 50 |
| Flip-Flop (M3) | 30 |
| Routing (M4) | <1 |
| BRAM (M6) | 120 |
| DSP (M7) | 570 |
| HDL Pipeline | 37 |
| 4K LUT Benchmark | 36 |
| **Total** | **2,251 ms** |

---

## Test Results

| Test Suite | Result |
|------------|--------|
| Self-Tests (M0) | 6/6 PASS |
| Bit-Parallel (M1) | 10/10 PASS |
| LUT4 (M2) | 13/13 PASS |
| Flip-Flop (M3) | 6/6 PASS |
| Routing (M4) | 5/5 PASS |
| BRAM (M6) | 3/3 PASS |
| DSP (M7) | 4/4 PASS |
| HDL Pipeline | 3/3 PASS |
| RISC-V Demo | 1/1 PASS |
| **Total** | **51/51 PASS** |

---

## Memory Usage

| Component | Internal RAM | PSRAM |
|-----------|--------------|-------|
| VFPGA Core (4K config) | 8.4 KB | 400 KB |
| Free | 328 KB | 7.8 MB |
| Total Internal | 336 KB | — |
| Total PSRAM | — | 8 MB |

---

## Capacity Context (not a speed comparison)

| FPGA | LUT4s | Equivalent |
|------|-------|------------|
| **VFPGA-S3** | **4,096** | **—** |
| Lattice iCE40 LP384 | 384 | 0.1x |
| Lattice iCE40 UP5K | 5,280 | 1.3x (closest capacity reference) |
| Gowin GW1NR-9 | 8,640 | 2.1x |
| Xilinx Spartan-7 XC7S25 | 15,000 | 3.7x |

The VFPGA-S3 does not compete with silicon FPGAs on clock speed. Its value proposition is reconfigurability, zero hardware barrier to entry, and software-defined logic on a low-cost microcontroller.

---

## Performance Optimizations

| Optimization | Speedup | Notes |
|--------------|---------|-------|
| Lookup table (branchless) | ~50x | Eliminates branch mispredictions |
| PIE batch (4 LUTs) | ~4x | Processes 4 LUTs per iteration |
| Heap allocation (PSRAM) | — | Enables 4K+ LUT configs |
| Bit-parallel (32-wide) | 32x | Processes 32 signals per op |

---

## Engine Profile (measurement only)

`Benchmark::run_profile()` splits one eval cycle into LUT batch vs FF update vs signal I/O at 64/1,024/4,096 LUTs (serial output). It instruments only — no evaluation code path is modified. Run it to locate scaling bottlenecks (routing vs LUT eval vs FF updates) before attempting engine changes.

---

## Fabric Benchmark Suite (golden vectors)

`Benchmark::run_fabric_suite()` runs real digital circuits with known-good outputs:

| Design | Resources | Golden vector |
|--------|-----------|---------------|
| AND-net64 | 64 LUTs | all-1 inputs → 1 |
| Counter8 | ~36 LUTs, 8 FFs | 300 cycles → 44 |
| LFSR8 | 3 LUTs, 8 FFs | C++ reference model |
| UART-TX 8N1 | 10 FFs | 0x55 frame bits |
| FIR-4tap | DSP MAC | 300 |
| CRC-8 | fabric XOR gates | 0xF4 (`"123456789"`) |
| AES-round | fabric XOR + S-box | ARK=0x78, SBOX=0xBC |

Each line reports LUT/FF cost, latency, throughput, and PASS/FAIL.

---

## Throughput vs. Real Hardware

| Metric | VFPGA-S3 (Software) | iCE40 UP5K (Hardware) | Ratio |
|--------|---------------------|----------------------|-------|
| LUT4s | 4,096 | 5,280 | 0.77x |
| Clock | 28 Keval/s | 48 MHz | 0.0006x |
| Logic eval | 36 us/cycle | 21 ns/cycle | 1,714x slower |
| DSP | 8.3 Mops/s (software MAC) | 8 x 16-bit MACs (silicon) | Not directly comparable |

The VFPGA-S3 runs at ~1,700x slower than a real FPGA for combinational logic, but provides full programmability in software with no physical hardware constraints.
