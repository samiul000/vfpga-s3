# PROJECT: VFPGA-S3 SOFTWARE-DEFINED VIRTUAL FPGA ON ESP32-S3 N16R8

You are a senior embedded-systems engineer specializing in:

- ESP32-S3
- ESP-IDF
- FreeRTOS
- Embedded C/C++
- Digital logic
- FPGA architecture
- FPGA LUT/FF structures
- Bit-parallel computing
- SIMD/vector processing
- DMA
- Memory optimization
- Embedded benchmarking
- CPU emulation
- RISC-V
- Hardware/software co-design

Your task is to design and implement a working:

> Software-Defined Virtual FPGA (VFPGA)

running on:

> ESP32-S3 N16R8

installed on:

> ESP32-S3-DevKitC-1

The project should experimentally determine how much configurable,
FPGA-like parallel digital computation can be achieved through software
on the ESP32-S3.

---

# 1. IMPORTANT TERMINOLOGY

Do NOT describe the ESP32-S3 as containing a physical FPGA.

Use terminology such as:

- Software-defined FPGA
- Virtual FPGA
- VFPGA
- FPGA emulator
- Configurable software logic fabric
- Software-configurable digital logic fabric

Do NOT claim that the ESP32-S3 contains physical FPGA LUTs,
flip-flops, routing fabric, or programmable FPGA interconnect.

The project is intended to emulate FPGA-like configurable logic and
exploit software parallelism available on the ESP32-S3.

---

# 2. AUTHORITATIVE HARDWARE INFORMATION

The physical target is:

- ESP32-S3 N16R8
- ESP32-S3-DevKitC-1

Use official Espressif documentation as the authoritative source for:

- GPIO availability
- GPIO alternate functions
- flash/PSRAM connections
- USB
- UART
- JTAG
- strapping pins
- boot behavior
- peripheral conflicts
- ESP32-S3 architecture
- ESP-IDF APIs

Official references:

https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/

https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/

https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html

Never guess hardware capabilities.

If information is uncertain:

1. Verify using Espressif documentation.
2. Inspect the actual ESP-IDF configuration.
3. Inspect the detected hardware where possible.
4. If still uncertain, mark the resource as UNKNOWN/RESERVED.

Never fabricate a capability.

---

# 3. HARDWARE TARGET

Target board:

ESP32-S3-DevKitC-1

Target module:

ESP32-S3-WROOM N16R8

Expected configuration:

- ESP32-S3
- Dual-core Xtensa LX7
- Up to 240 MHz
- 16 MB flash
- 8 MB octal PSRAM

These values must NOT be blindly hard-coded.

At runtime detect and report:

- Chip model
- Chip revision
- CPU frequency
- Number of cores
- Flash size
- PSRAM size
- Internal RAM
- Free heap
- Free PSRAM

---

# 4. PRIMARY OBJECTIVE

Build a VFPGA engine capable of representing:

- LUTs
- Flip-flops
- Multiplexers
- Routing
- Virtual I/O
- Virtual BRAM
- Virtual DSP blocks
- Virtual clock
- Configurable digital circuits

The VFPGA should be capable of executing circuits such as:

- AND gates
- OR gates
- XOR networks
- Counters
- LFSRs
- UART transmitters
- FIR filters
- Small neural networks
- Other configurable digital logic

---

# 5. CORE CONCEPT

The system should resemble:

                    VFPGA

                     │
              Virtual I/O
                     │
              Virtual Routing
                     │
        ┌────────────┼────────────┐
        │            │            │
       LUT           FF          DSP
        │            │            │
        └────────────┼────────────┘
                     │
              Virtual Outputs
                     │
              Physical I/O
                     │
                ESP32 GPIO


The important difference is:

A physical FPGA executes these structures using actual hardware logic.

The ESP32-S3 implementation executes them using software.

---

# 6. PROJECT ARCHITECTURE

Implement:

                    ESP32-S3 N16R8
                           │
             ┌─────────────┴─────────────┐
             │                           │
          Core 0                       Core 1
             │                           │
      Control / RTOS              VFPGA execution
             │                           │
             └──────────────┬────────────┘
                            │
                       VFPGA Engine
                            │
              ┌─────────────┼─────────────┐
              │             │             │
             LUT            FF           DSP
              │             │             │
              └─────────────┼─────────────┘
                            │
                       Virtual Routing
                            │
                       Virtual I/O
                            │
                       GPIO Bridge
                            │
                     Physical GPIO
                            │
                    External Hardware

---

# 7. ESP32-S3 N16R8 GPIO SAFETY

This is a critical requirement.

Do NOT treat every ESP32-S3 GPIO as available for VFPGA physical I/O.

The VFPGA must protect:

- Flash interface
- Octal PSRAM interface
- USB
- USB-JTAG
- UART programming/debugging
- Boot/strapping pins
- Reset/EN
- Power pins
- Board-specific resources
- Onboard RGB LED unless explicitly selected

The default VFPGA GPIO allocator must only select verified-safe pins.

If uncertain whether a GPIO is safe:

> DO NOT USE IT.

---

# 8. DEFAULT SAFE GPIO POOL

For the initial implementation, use this conservative GPIO pool:

GPIO1
GPIO2
GPIO4
GPIO5
GPIO6
GPIO7
GPIO8
GPIO9
GPIO10
GPIO11
GPIO12
GPIO13
GPIO14
GPIO16
GPIO17
GPIO18
GPIO21
GPIO38
GPIO39
GPIO40
GPIO41
GPIO42

GPIO47 may be treated as OPTIONAL and must be verified against the
actual board/module configuration before automatic use.

The allocator must not automatically use GPIOs outside this pool.

---

# 9. PROTECTED GPIOs

The following GPIOs must NOT be automatically assigned to VFPGA
physical I/O.

## GPIO0

Status:

RESERVED_BOOT

Reason:

Boot/strapping functionality.

Do not automatically use GPIO0.

---

## GPIO3

Status:

RESERVED_STRAPPING

Do not automatically use GPIO3.

---

## GPIO15

Status:

RESERVED_SPECIAL

Do not include GPIO15 in the default automatic GPIO allocator.

It may be manually enabled only after verifying the actual ESP-IDF
configuration and board behavior.

---

## GPIO19

Status:

RESERVED_USB

GPIO19 is associated with the ESP32-S3 USB interface.

Do not automatically use it.

---

## GPIO20

Status:

RESERVED_USB

GPIO20 is associated with the ESP32-S3 USB interface.

Do not automatically use it.

---

## GPIO35

Status:

RESERVED_PSRAM

CRITICAL:

For ESP32-S3 N16R8, GPIO35 is associated with the octal PSRAM
interface.

Do NOT use GPIO35 for VFPGA physical I/O.

---

## GPIO36

Status:

RESERVED_PSRAM

For ESP32-S3 N16R8, GPIO36 is associated with the octal PSRAM
interface.

Do NOT use GPIO36 for VFPGA physical I/O.

---

## GPIO37

Status:

RESERVED_PSRAM

For ESP32-S3 N16R8, GPIO37 is associated with the octal PSRAM
interface.

Do NOT use GPIO37 for VFPGA physical I/O.

---

## GPIO43

Status:

RESERVED_UART0

GPIO43 is associated with UART0 TX.

Do not automatically assign it to VFPGA physical I/O.

---

## GPIO44

Status:

RESERVED_UART0

GPIO44 is associated with UART0 RX.

Do not automatically assign it to VFPGA physical I/O.

---

## GPIO45

Status:

RESERVED_STRAPPING

Do not automatically use GPIO45.

---

## GPIO46

Status:

RESERVED_STRAPPING

Do not automatically use GPIO46.

---

## GPIO48

Status:

RESERVED_BOARD_LED

GPIO48 is associated with the onboard RGB LED on the DevKitC-1.

Do not use GPIO48 as a normal VFPGA GPIO by default.

It may be exposed as a special resource:

BOARD_RGB_LED

---

# 10. GPIO26–34

Treat GPIO26–34 as RESERVED/INTERNAL for this project unless their
availability for external use on the exact target configuration has
been explicitly verified.

Do not include them in the default VFPGA GPIO allocator.

---

# 11. GPIO39–42

GPIO39–42 have JTAG-related alternate functions.

They are not automatically considered unavailable.

However, the board configuration must determine whether JTAG/debugging
is currently using them.

If JTAG is active:

    GPIO39–42 → RESERVED_DEBUG

If JTAG is not using them and the pins are verified available:

    GPIO39–42 → OPTIONAL_SAFE

Do not break debugging functionality merely to obtain additional VFPGA
GPIOs.

---

# 12. GPIO47

GPIO47 is optional.

Before using GPIO47, verify:

- Exact ESP32-S3 module configuration
- Board configuration
- Voltage domain
- Active peripheral assignments
- Electrical safety

If verified:

    GPIO47 → OPTIONAL_SAFE

Otherwise:

    GPIO47 → RESERVED

---

# 13. GPIO STATUS CATEGORIES

Every GPIO must have one of:

SAFE
OPTIONAL
RESERVED
UNKNOWN

Example:

GPIO4:

    SAFE

GPIO19:

    RESERVED_USB

GPIO35:

    RESERVED_PSRAM

GPIO43:

    RESERVED_UART0

GPIO48:

    RESERVED_BOARD_LED

Never silently assign UNKNOWN pins.

---

# 14. GPIO CAPABILITY DATABASE

Create:

boards/
└── esp32s3-devkitc1-n16r8/
    ├── gpio_map.json
    └── board_config.json

Each GPIO should contain metadata.

Example:

{
    "gpio": 4,
    "exposed": true,
    "input_capable": true,
    "output_capable": true,
    "adc_capable": true,
    "touch_capable": true,
    "pwm_capable": true,
    "usb_related": false,
    "jtag_related": false,
    "uart_related": false,
    "strapping_pin": false,
    "flash_related": false,
    "psram_related": false,
    "board_led": false,
    "reserved": false,
    "safe_default": true
}

The exact values must be verified.

Do not fabricate GPIO capabilities.

---

# 15. VIRTUAL I/O

The VFPGA must support significantly more virtual I/O signals than
physical GPIO pins.

Support:

    32 VIO
    64 VIO
    128 VIO
    256 VIO

Example:

    VIO0
    VIO1
    VIO2
    ...
    VIO127

Only a subset needs physical GPIO mappings.

Example:

    VIO0 → GPIO4
    VIO1 → GPIO5
    VIO2 → GPIO6
    VIO3 → GPIO7

while:

    VIO4 → internal
    VIO5 → internal
    ...
    VIO127 → internal

Therefore:

    VIRTUAL I/O COUNT != PHYSICAL GPIO COUNT

---

# 16. GPIO BRIDGE

Implement:

class GPIOBridge
{
public:

    bool map_input(
        uint16_t virtual_io,
        uint8_t physical_gpio
    );

    bool map_output(
        uint16_t virtual_io,
        uint8_t physical_gpio
    );

    bool unmap(
        uint16_t virtual_io
    );

    bool validate_mapping(
        uint16_t virtual_io,
        uint8_t physical_gpio
    );

    void sample_inputs();

    void commit_outputs();
};

Only GPIOBridge may access physical GPIO hardware.

The VFPGA logic must NEVER directly call:

gpio_get_level()

gpio_set_level()

or directly manipulate ESP32 GPIO registers.

---

# 17. GPIO MAPPING SEPARATION

The VFPGA logic must only see:

    VIO0
    VIO1
    VIO2

It must not know:

    GPIO4
    GPIO5
    GPIO6

The mapping must exist as a separate layer.

Architecture:

    VFPGA
       │
    Virtual I/O
       │
    GPIO Bridge
       │
    GPIO Mapper
       │
    ESP32 GPIO HAL
       │
    Physical GPIO

This allows the same VFPGA configuration to run with different
physical GPIO assignments.

---

# 18. GPIO MAPPING VALIDATION

Before mapping:

    VIO → GPIO

validate:

1. GPIO exists.
2. GPIO is physically exposed.
3. GPIO is not reserved.
4. GPIO is not PSRAM.
5. GPIO is not flash.
6. GPIO is not USB.
7. GPIO is not UART.
8. GPIO is not boot-critical.
9. GPIO is not strapping-critical.
10. GPIO is not already allocated.
11. GPIO supports requested direction.
12. GPIO is not currently required by another ESP-IDF peripheral.

Example:

ERROR:

    VIO7 → GPIO35

Reason:

    GPIO35 is reserved for octal PSRAM on ESP32-S3 N16R8.

Reject the mapping.

---

# 19. GPIO AUTOMATIC ALLOCATION

Implement:

uint8_t select_best_gpio(
    VIORequirements requirements
);

Priority:

1. SAFE
2. General-purpose
3. Not allocated
4. No peripheral conflict
5. Correct direction
6. No board functionality conflict

Never automatically allocate:

- PSRAM
- Flash
- USB
- UART0
- Boot pins
- Strapping pins
- Reset
- Power
- Reserved resources

---

# 20. BULK GPIO SAMPLING

Investigate ESP32-S3 GPIO register access.

Avoid unnecessarily performing:

gpio_get_level()
gpio_get_level()
gpio_get_level()
gpio_get_level()

for every virtual signal.

Where safe, use bulk GPIO register reads and bit masks.

Concept:

    Physical GPIOs
          │
          ▼
    GPIO register
          │
          ▼
       Bit mask
          │
          ▼
    VFPGA input word

Benchmark:

    Individual GPIO reads
        VS
    Bulk GPIO reads

---

# 21. PHYSICAL INPUT PATH

Implement:

    Physical GPIO
          ↓
       GPIO HAL
          ↓
      GPIO Bridge
          ↓
    Physical-to-Virtual Mapper
          ↓
      Virtual Input
          ↓
      VFPGA Fabric

---

# 22. PHYSICAL OUTPUT PATH

Implement:

    VFPGA Fabric
          ↓
     Virtual Output
          ↓
      GPIO Bridge
          ↓
      Physical GPIO
          ↓
    External Hardware

---

# 23. PHYSICAL OUTPUT COMMIT MODES

Support:

    EVERY_VIRTUAL_CYCLE
    EVERY_N_CYCLES
    EXPLICIT_COMMIT
    REALTIME_IO

Default:

    EXPLICIT_COMMIT

Do not continuously write physical GPIOs unless required.

---

# 24. REAL-TIME I/O MODE

Implement:

    REALTIME_IO

Pipeline:

    Physical input transition
            ↓
      GPIO sampling
            ↓
      VFPGA execution
            ↓
      Output generation
            ↓
      GPIO output commit

Measure:

    End-to-end software I/O latency

Do NOT call this:

    FPGA propagation delay

It is software I/O latency.

---

# 25. VFPGA DATA REPRESENTATION

Use bit-parallel representation.

Initially:

using VSignal = uint32_t;

One bit represents one independent Boolean signal.

Example:

    bit 0  = signal 0
    bit 1  = signal 1
    ...
    bit 31 = signal 31

This allows 32 independent Boolean signals to be processed through
one CPU integer operation.

Later investigate:

- uint64_t
- packed bitsets
- SIMD/vector registers

based on actual ESP32-S3 capabilities and compiler support.

---

# 26. BIT-PARALLEL LOGIC ENGINE

Implement:

    AND
    OR
    XOR
    NOT
    NAND
    NOR
    XNOR
    MUX

Example:

    y = a & b;

The implementation should process many independent Boolean signals
in parallel using packed bits.

---

# 27. LUT4

Implement configurable 4-input LUTs.

Each LUT4 contains:

    4 inputs
    16 truth-table entries
    1 output

Store truth table as:

uint16_t

Implement:

class VLUT4
{
public:

    void configure(uint16_t truth_table);

    VSignal evaluate(
        VSignal a,
        VSignal b,
        VSignal c,
        VSignal d
    );
};

Test against complete LUT4 truth tables.

---

# 28. VIRTUAL FLIP-FLOPS

Implement D flip-flops.

State:

    D
    Q
    RESET
    ENABLE

Use:

    current_state
    next_state

Execution order:

1. Sample inputs.
2. Evaluate combinational logic.
3. Calculate next state.
4. Generate virtual clock edge.
5. Commit next state.

---

# 29. VIRTUAL MUX

Implement:

    2:1 MUX
    4:1 MUX
    configurable N:1 MUX

Use bit-parallel operations where possible.

---

# 30. VIRTUAL ROUTING

Create a signal/net system.

Each virtual signal should have an integer ID.

Prefer:

    contiguous arrays
    packed structures
    cache-friendly data

Avoid excessive dynamic allocation.

Benchmark:

    Array-based routing
        VS
    Object-based routing

---

# 31. VFPGA CORE

Implement:

class VFPGA
{
public:

    void initialize();

    void reset();

    bool load_config();

    void evaluate_combinational();

    void clock();

    void run_cycles(uint32_t cycles);

    VSignal read_input(uint16_t id);

    void write_input(uint16_t id, VSignal value);

    VSignal read_output(uint16_t id);
};

---

# 32. VIRTUAL BRAM

Implement configurable virtual BRAM.

Initial configurations:

    64 × 32-bit
    256 × 32-bit
    1024 × 32-bit

Support:

    read()
    write()
    reset()

Large BRAM structures may use PSRAM if benchmarking demonstrates
acceptable performance.

Hot data should remain in internal SRAM whenever possible.

---

# 33. VIRTUAL DSP

Implement:

    A × B + C

Support:

    INT8
    INT16
    INT32

Benchmark:

    Scalar
        VS
    Optimized implementation
        VS
    SIMD/vector implementation

Only keep optimizations that produce measured improvements.

---

# 34. VIRTUAL CLOCK

Implement deterministic execution:

    Input sampling
          ↓
    Combinational evaluation
          ↓
    Next-state calculation
          ↓
    Clock edge
          ↓
    State commit
          ↓
    Next virtual cycle

Measure:

    Virtual cycles/second

Do not equate virtual cycles/second with physical FPGA clock frequency.

---

# 35. SOFTWARE PARALLELISM

The major research objective is to investigate software parallelism.

Implement and benchmark:

    Scalar execution
    Bit-parallel execution
    SIMD/vector execution
    Dual-core execution
    Bit-parallel + SIMD
    Bit-parallel + dual-core
    SIMD + dual-core
    Bit-parallel + SIMD + dual-core

Measure actual performance.

Do not assume that adding parallelism always improves performance.

---

# 36. SIMD / VECTOR OPTIMIZATION

Investigate the ESP32-S3 vector instruction capabilities.

Do not invent intrinsic names.

Verify:

- compiler support
- ESP-IDF support
- Xtensa vector instructions
- alignment requirements
- supported data types

Create separate implementations where appropriate:

    scalar_engine
    bitparallel_engine
    simd_engine

Benchmark all versions.

---

# 37. DUAL-CORE EXECUTION

Use FreeRTOS.

Initial architecture:

    Core 0:
        control
        configuration
        physical I/O

    Core 1:
        VFPGA execution
        benchmark execution

Benchmark:

    single-core
        VS
    dual-core

Measure:

- execution time
- throughput
- CPU utilization
- cross-core communication overhead
- synchronization overhead

Do not assume dual-core execution is faster.

---

# 38. MEMORY STRATEGY

Keep hot execution data in internal SRAM:

- Virtual signal state
- LUT state
- FF state
- Routing state
- Scheduler state
- Frequently accessed configuration

Use PSRAM for:

- Large VFPGA configurations
- Large virtual BRAM
- Benchmark datasets
- Optional RISC-V memory
- Large lookup tables

Do not place performance-critical data in PSRAM without measuring the
performance impact.

---

# 39. PROJECT DIRECTORY

Create:

vfpga_s3/
│
├── CMakeLists.txt
├── sdkconfig.defaults
├── README.md
│
├── main/
│   ├── CMakeLists.txt
│   ├── main.cpp
│
│   ├── vfpga/
│   │   ├── vfpga_core.h
│   │   ├── vfpga_core.cpp
│   │   ├── vfpga_config.h
│   │   ├── vfpga_config.cpp
│   │   ├── vfpga_lut.h
│   │   ├── vfpga_lut.cpp
│   │   ├── vfpga_ff.h
│   │   ├── vfpga_ff.cpp
│   │   ├── vfpga_mux.h
│   │   ├── vfpga_mux.cpp
│   │   ├── vfpga_bram.h
│   │   ├── vfpga_bram.cpp
│   │   ├── vfpga_dsp.h
│   │   ├── vfpga_dsp.cpp
│   │   ├── vfpga_io.h
│   │   ├── vfpga_io.cpp
│   │   ├── vfpga_scheduler.h
│   │   └── vfpga_scheduler.cpp
│
│   ├── engine/
│   │   ├── bitparallel.h
│   │   ├── bitparallel.cpp
│   │   ├── simd_engine.h
│   │   ├── simd_engine.cpp
│   │   ├── execution_engine.h
│   │   └── execution_engine.cpp
│
│   ├── io/
│   │   ├── gpio_bridge.h
│   │   ├── gpio_bridge.cpp
│   │   ├── gpio_capability.h
│   │   ├── gpio_capability.cpp
│   │   ├── board_profile.h
│   │   └── board_profile.cpp
│
│   ├── hdl/
│   │   ├── lexer.h
│   │   ├── lexer.cpp
│   │   ├── parser.h
│   │   ├── parser.cpp
│   │   ├── netlist.h
│   │   ├── netlist.cpp
│   │   ├── mapper.h
│   │   └── mapper.cpp
│
│   ├── riscv/
│   │   ├── riscv_cpu.h
│   │   ├── riscv_cpu.cpp
│   │   ├── riscv_memory.h
│   │   ├── riscv_memory.cpp
│   │   └── riscv_decoder.cpp
│
│   └── benchmarks/
│       ├── benchmark.h
│       ├── benchmark.cpp
│       ├── logic_benchmark.cpp
│       ├── counter_benchmark.cpp
│       ├── lfsr_benchmark.cpp
│       └── nn_benchmark.cpp
│
├── boards/
│   └── esp32s3-devkitc1-n16r8/
│       ├── gpio_map.json
│       └── board_config.json
│
├── tools/
│   └── vfpga_compiler/
│
├── examples/
│   ├── and_gate.vhdl
│   ├── counter.vhdl
│   ├── lfsr.vhdl
│   └── uart.vhdl
│
└── tests/

---

# 40. BUILD SYSTEM

Use:

    ESP-IDF
    C/C++
    FreeRTOS
    CMake

Do not use Arduino as the primary framework.

Build:

    idf.py build

Flash:

    idf.py flash

Monitor:

    idf.py monitor

---

# 41. MILESTONE 0 — HARDWARE DIAGNOSTIC

First implement a hardware diagnostic.

Print:

    VFPGA Hardware Diagnostic

    Board:
    Chip:
    Revision:
    CPU:
    Cores:
    CPU frequency:
    Flash:
    PSRAM:
    Internal RAM:
    Free heap:
    Free PSRAM:

    GPIO capability database:
    Loaded

    Protected GPIOs:
    ...

    Safe GPIOs:
    ...

Test:

- PSRAM allocation
- Internal RAM allocation
- Safe GPIO input
- Safe GPIO output
- Timer
- Core 0
- Core 1

Do not use protected GPIOs.

---

# 42. MILESTONE 1 — BIT-PARALLEL ENGINE

Implement:

    AND
    OR
    XOR
    NOT
    NAND
    NOR
    XNOR
    MUX

Create automated tests.

Verify results against reference Boolean operations.

---

# 43. MILESTONE 2 — LUT4

Implement configurable LUT4.

Test all 2^16 possible LUT truth tables where practical.

At minimum test:

- AND
- OR
- XOR
- NAND
- NOR
- XNOR
- Multiplexer
- Constant 0
- Constant 1

---

# 44. MILESTONE 3 — FLIP-FLOPS

Implement:

- D flip-flop
- Enable
- Reset
- Clock

Test:

- reset
- rising edge
- disabled state
- state retention

---

# 45. MILESTONE 4 — ROUTING

Implement:

- virtual nets
- signal IDs
- input routing
- output routing
- fan-out

Benchmark routing overhead.

---

# 46. MILESTONE 5 — VFPGA CORE

Integrate:

- LUT
- FF
- MUX
- routing
- virtual I/O
- virtual clock

Run:

- AND gate
- XOR gate
- counter
- LFSR

---

# 47. MILESTONE 6 — VIRTUAL BRAM

Implement:

    64 × 32-bit
    256 × 32-bit
    1024 × 32-bit

Benchmark:

- SRAM
- PSRAM

if large memory is involved.

---

# 48. MILESTONE 7 — VIRTUAL DSP

Implement:

    INT8 multiply
    INT16 multiply
    INT32 multiply
    A × B + C

Benchmark all implementations.

---

# 49. MILESTONE 8 — PHYSICAL GPIO

Implement GPIOBridge.

Demonstrate:

    Physical button
          ↓
       GPIO4
          ↓
     GPIOBridge
          ↓
        VIO0
          ↓
       LUT/logic
          ↓
        VIO1
          ↓
     GPIOBridge
          ↓
       GPIO5
          ↓
        LED

Use only verified safe GPIOs.

---

# 50. MILESTONE 9 — VIRTUAL COUNTER

Create:

    Button
       ↓
    Physical GPIO
       ↓
    VIO
       ↓
    Virtual counter
       ↓
    8-bit virtual output
       ↓
    GPIOBridge
       ↓
    Physical LEDs

Automatically allocate only safe GPIOs.

---

# 51. MILESTONE 10 — SIMD

Create optimized SIMD/vector execution.

Benchmark:

    Scalar
    Bit-parallel
    SIMD

Report:

    latency
    throughput
    speedup
    CPU utilization

---

# 52. MILESTONE 11 — DUAL CORE

Run the VFPGA engine on one core and control/I/O on another.

Benchmark:

    Single-core
        VS
    Dual-core

---

# 53. MILESTONE 12 — CONFIGURATION FORMAT

Create compact VFPGA binary format.

Header:

    MAGIC
    VERSION
    LUT_COUNT
    FF_COUNT
    BRAM_SIZE
    DSP_COUNT
    VIO_COUNT

Followed by:

    LUT configuration
    FF configuration
    routing
    BRAM initialization
    DSP configuration
    VIO configuration

Validate:

- magic
- version
- checksum
- resource limits
- signal IDs
- LUT IDs
- FF IDs
- memory addresses

---

# 54. MILESTONE 13 — MINI HDL

Implement a minimal HDL.

Example:

    module and_gate;

    input a;
    input b;
    output y;

    assign y = a & b;

    endmodule

Support:

    module
    input
    output
    wire
    assign
    register
    clock
    &
    |
    ^
    !

Pipeline:

    HDL
      ↓
    Lexer
      ↓
    Parser
      ↓
    AST
      ↓
    Netlist
      ↓
    LUT mapping
      ↓
    VFPGA configuration

Do not attempt to implement full Verilog.

---

# 55. LUT MAPPING

Implement:

    Boolean network
          ↓
      LUT4 mapping
          ↓
     VFPGA LUT fabric

Initially use a simple greedy mapper.

Later investigate:

- constant propagation
- Boolean simplification
- dead logic elimination
- common subexpression elimination
- LUT packing

---

# 56. MILESTONE 14 — BENCHMARK SUITE

Benchmark:

1. AND network
2. XOR network
3. 32-bit counter
4. LFSR
5. UART transmitter
6. FIR filter
7. INT8 neural network

Measure:

- LUT count
- FF count
- BRAM
- DSP
- virtual cycles/s
- latency
- CPU utilization
- internal RAM
- PSRAM
- flash usage

---

# 57. MILESTONE 15 — INT8 NEURAL NETWORK

Implement:

    Input
      ↓
    Dense INT8
      ↓
    ReLU
      ↓
    Dense INT8
      ↓
    Output

Compare:

    Standard ESP32-S3 implementation
        VS
    VFPGA virtual DSP implementation

Measure:

- inference latency
- throughput
- RAM
- PSRAM
- CPU utilization

---

# 58. MILESTONE 16 — RISC-V EMULATOR

Only implement after the VFPGA engine is stable.

Implement a minimal RV32I emulator.

Initially support:

    ADD
    SUB
    AND
    OR
    XOR
    SLL
    SRL
    SRA
    ADDI
    LW
    SW
    BEQ
    BNE
    JAL
    JALR
    LUI
    AUIPC

Implement:

    32 registers
    PC
    memory
    decoder
    execution

---

# 59. RISC-V + VFPGA

Eventually implement:

                    VFPGA
                       │
             ┌─────────┴─────────┐
             │                   │
         RV32I CPU           VFPGA Fabric
             │                   │
             ├── RAM             ├── LUT
             ├── GPIO            ├── FF
             ├── UART            ├── BRAM
             └── Control         └── DSP

The RISC-V processor must be able to:

- Configure VFPGA
- Start VFPGA
- Stop VFPGA
- Read VFPGA status
- Read VFPGA outputs
- Write VFPGA inputs

---

# 60. RISC-V MEMORY MAP

Define a formal memory map.

Example:

    0x00000000 → RISC-V RAM
    0x10000000 → VFPGA GPIO
    0x10001000 → VFPGA status
    0x10002000 → VFPGA configuration
    0x10003000 → VFPGA control

These are examples.

The final implementation must formally define and validate the actual
memory map.

Do not create address conflicts.

---

# 61. PHYSICAL GPIO DEMONSTRATION

At minimum implement:

## Demo 1 — Combinational

    Button
       ↓
    GPIO4
       ↓
    VIO0
       ↓
    Virtual LUT
       ↓
    VIO1
       ↓
    GPIO5
       ↓
    LED

---

## Demo 2 — Sequential

    Button
       ↓
    GPIO
       ↓
    VFPGA
       ↓
    Counter
       ↓
    Virtual output bus
       ↓
    GPIOBridge
       ↓
    Multiple LEDs

---

## Demo 3 — HDL

    Mini HDL
       ↓
    Compiler
       ↓
    Netlist
       ↓
    VFPGA configuration
       ↓
    ESP32-S3
       ↓
    Physical I/O

---

## Demo 4 — RISC-V

    RISC-V program
       ↓
    RISC-V emulator
       ↓
    Memory-mapped VFPGA
       ↓
    Virtual hardware
       ↓
    Physical GPIO

---

## Demo 5 — Neural Network

    INT8 neural network
       ↓
    Virtual DSP
       ↓
    Benchmark

---

# 62. PERFORMANCE BENCHMARKING

Generate:

| Implementation | Latency | Throughput | CPU Usage | RAM | PSRAM |
|---|---:|---:|---:|---:|---:|
| Scalar | | | | | |
| Bit-parallel | | | | | |
| SIMD | | | | | |
| Dual-core | | | | | |
| Bit-parallel + SIMD | | | | | |
| Bit-parallel + dual-core | | | | | |
| SIMD + dual-core | | | | | |
| Full optimized | | | | | |

Only populate actual measured values.

Never fabricate benchmark results.

---

# 63. VFPGA RESOURCE BENCHMARK

Generate:

| Benchmark | LUTs | FFs | BRAM | DSP | Virtual cycles/s | Latency |
|---|---:|---:|---:|---:|---:|---:|
| AND | | | | | | |
| XOR | | | | | | |
| Counter | | | | | | |
| LFSR | | | | | | |
| UART | | | | | | |
| FIR | | | | | | |
| INT8 NN | | | | | | |

---

# 64. PHYSICAL I/O BENCHMARK

Measure separately:

1. GPIO sampling latency
2. VFPGA execution latency
3. GPIO output commit latency
4. End-to-end input-to-output latency
5. Maximum reliable physical I/O update rate
6. Virtual cycles/second
7. Physical GPIO updates/second
8. CPU utilization

Do not combine these measurements into one number.

---

# 65. IMPORTANT PERFORMANCE DISTINCTION

Clearly distinguish:

    Virtual FPGA throughput

from:

    Physical GPIO throughput

For example:

    Virtual logic may execute many cycles per second,

while:

    Physical GPIO updates may be substantially slower.

Do not claim that virtual cycle rate equals physical FPGA clock speed.

---

# 66. TESTING

Create automated tests for:

- LUT
- FF
- MUX
- BRAM
- DSP
- routing
- GPIO mapping
- configuration loader
- HDL parser
- HDL compiler
- RISC-V decoder
- RISC-V execution

GPIO safety tests must verify that the following are rejected by
default:

    GPIO0
    GPIO3
    GPIO15
    GPIO19
    GPIO20
    GPIO35
    GPIO36
    GPIO37
    GPIO43
    GPIO44
    GPIO45
    GPIO46
    GPIO48

The following must be accepted only if the board profile verifies them:

    GPIO39
    GPIO40
    GPIO41
    GPIO42
    GPIO47

---

# 67. BOARD FUNCTIONALITY PROTECTION

The firmware must preserve:

- Booting
- Flash operation
- PSRAM operation
- Reset
- USB functionality
- UART programming/debugging
- Wi-Fi
- Bluetooth
- FreeRTOS
- ESP-IDF operation

Do not sacrifice board functionality simply to obtain additional
physical VFPGA I/O.

---

# 68. DEVELOPMENT MODE

Development mode should preserve:

- UART logging
- USB
- JTAG
- debugging
- programming functionality

Only use the default safe GPIO pool.

---

# 69. PERFORMANCE MODE

Performance mode may disable unnecessary:

- logging
- debugging
- USB functions
- UART console

only when explicitly configured.

Even in performance mode, protect:

- flash
- PSRAM
- boot-critical resources
- required board functions

---

# 70. GPIO COMMAND

Implement:

    gpio list

Example:

    ESP32-S3-DevKitC-1 N16R8

    GPIO1   SAFE
    GPIO2   SAFE
    GPIO4   SAFE
    GPIO5   SAFE
    GPIO6   SAFE
    GPIO7   SAFE
    GPIO8   SAFE
    GPIO9   SAFE
    GPIO10  SAFE
    GPIO11  SAFE
    GPIO12  SAFE
    GPIO13  SAFE
    GPIO14  SAFE

    GPIO15  RESERVED

    GPIO16  SAFE
    GPIO17  SAFE
    GPIO18  SAFE

    GPIO19  RESERVED_USB
    GPIO20  RESERVED_USB

    GPIO21  SAFE

    GPIO35  RESERVED_PSRAM
    GPIO36  RESERVED_PSRAM
    GPIO37  RESERVED_PSRAM

    GPIO38  SAFE
    GPIO39  OPTIONAL
    GPIO40  OPTIONAL
    GPIO41  OPTIONAL
    GPIO42  OPTIONAL

    GPIO43  RESERVED_UART0
    GPIO44  RESERVED_UART0

    GPIO45  RESERVED_STRAPPING
    GPIO46  RESERVED_STRAPPING

    GPIO47  OPTIONAL
    GPIO48  RESERVED_BOARD_LED

The actual status must be generated from the verified board profile.

---

# 71. BOARD CONFIGURATION SEPARATION

The VFPGA configuration must not hard-code physical GPIO numbers.

Example:

    counter.vfpga

can run with:

    Mapping A:
        VIO0 → GPIO4
        VIO1 → GPIO5

or:

    Mapping B:
        VIO0 → GPIO16
        VIO1 → GPIO17

The VFPGA logic remains unchanged.

---

# 72. CONFIGURATION FLOW

Use:

    VFPGA Logic
          +
    Board Mapping
          ↓
    Runtime Configuration
          ↓
    GPIO Bridge
          ↓
    Physical Hardware

---

# 73. RUNTIME GPIO DIAGNOSTICS

At startup print:

    === VFPGA GPIO STATUS ===

    Safe GPIOs:
    ...

    Reserved GPIOs:
    ...

    PSRAM GPIOs:
    ...

    USB GPIOs:
    ...

    UART GPIOs:
    ...

    Strapping GPIOs:
    ...

    Board resources:
    ...

This makes accidental resource conflicts visible.

---

# 74. ERROR HANDLING

If a user attempts:

    map VIO7 GPIO35

return:

    ERROR: GPIO35 is reserved.

    Reason:
    ESP32-S3 N16R8 octal PSRAM interface.

    VFPGA physical I/O mapping rejected.

Do not automatically override the restriction.

---

# 75. FINAL ARCHITECTURE

The final system should resemble:

                    ESP32-S3 N16R8
                           │
              ┌────────────┴────────────┐
              │                         │
           CPU / RTOS              Physical GPIO
              │                         │
              │                    GPIO HAL
              │                         │
              │                 GPIO Capability DB
              │                         │
              └──────────┬──────────────┘
                         │
                    GPIO Bridge
                         │
                  Physical Mapper
                         │
                   Virtual I/O
                         │
                  Virtual Routing
                         │
            ┌────────────┼────────────┐
            │            │            │
           LUT           FF          DSP
            │            │            │
            └────────────┼────────────┘
                         │
                   VFPGA Engine
                         │
                   Virtual Clock
                         │
                 Configurable Logic

---

# 76. RESEARCH QUESTION

The final experiment must answer:

> How much configurable, FPGA-like parallel digital computation can be
> extracted from an ESP32-S3 N16R8 using bit-parallel execution, SIMD,
> dual-core processing, optimized memory organization, configurable
> software LUTs, virtual flip-flops, virtual BRAM, virtual DSP blocks,
> and software-defined physical I/O?

---

# 77. LIMITATIONS TO DOCUMENT

Explicitly document that this is NOT a physical FPGA.

Discuss:

- Software execution overhead
- Instruction execution
- Memory latency
- GPIO latency
- Scheduling overhead
- Interrupts
- Cache effects
- PSRAM latency
- FreeRTOS overhead
- Dual-core synchronization
- Lack of physical LUT routing
- Lack of true FPGA flip-flop fabric
- Lack of physical FPGA clock distribution
- Physical GPIO bandwidth limitations

---

# 78. FINAL REPORT

Generate a technical report containing:

## Hardware

- ESP32-S3 variant
- Board
- CPU
- CPU frequency
- Flash
- PSRAM
- Internal RAM

## VFPGA

- LUT count
- FF count
- BRAM
- DSP
- Virtual I/O
- Physical I/O

## Execution

- Scalar performance
- Bit-parallel performance
- SIMD performance
- Dual-core performance
- Combined optimization performance

## Physical I/O

- GPIO sampling latency
- VFPGA latency
- GPIO commit latency
- End-to-end latency
- Maximum physical I/O update rate

## RISC-V

- Supported instructions
- Instructions/second
- Memory usage
- Emulator overhead

## Neural Network

- Latency
- Throughput
- RAM
- PSRAM
- CPU utilization

## Limitations

Clearly distinguish this system from a physical FPGA.

---

# 79. DEVELOPMENT MILESTONES

Implement sequentially:

M0  Hardware diagnostic
M1  Bit-parallel engine
M2  LUT4
M3  Flip-flops
M4  Routing
M5  VFPGA core
M6  BRAM
M7  DSP
M8  Physical GPIO bridge
M9  Virtual clock
M10 SIMD
M11 Dual-core
M12 Configuration format
M13 Mini HDL
M14 Benchmarks
M15 INT8 neural network
M16 RISC-V emulator
M17 RISC-V + VFPGA
M18 Final demonstrations

---

# 80. MILESTONE REPORTING

After each milestone report:

    Milestone:
    Status:

    Files created:

    Files modified:

    Build:
    PASS / FAIL

    Tests:
    PASS / FAIL

    Benchmark:

    Memory usage:

    CPU usage:

    Known limitations:

    Next milestone:

Do not proceed to the next milestone if the current milestone has
unresolved correctness errors.

---

# 81. STRICT ANTI-HALLUCINATION RULE

Never fabricate:

- ESP32-S3 hardware capabilities
- GPIO capabilities
- PSRAM pin assignments
- flash pin assignments
- compiler intrinsics
- performance numbers
- memory usage
- benchmark results
- RISC-V performance
- SIMD capabilities
- peripheral behavior

If information is uncertain:

    STOP
    VERIFY
    DOCUMENT THE SOURCE
    THEN IMPLEMENT

If a requested feature is impossible or unsafe:

    STOP

Explain why.

Do not create a fake implementation and claim that it works.

---

# 82. FINAL SUCCESS CRITERIA

The project is considered successful when it demonstrates:

1. Configurable LUT-based software logic.
2. Virtual flip-flops.
3. Virtual routing.
4. Bit-parallel Boolean execution.
5. Virtual BRAM.
6. Virtual DSP.
7. Configurable virtual I/O.
8. Safe physical GPIO mapping.
9. Physical input → VFPGA → physical output.
10. Mini HDL → VFPGA configuration.
11. Measured bit-parallel speedup.
12. Measured SIMD/vector performance.
13. Measured dual-core performance.
14. Safe operation on ESP32-S3 N16R8.
15. Flash and PSRAM remain functional.
16. USB remains functional unless explicitly disabled.
17. UART programming/debugging remains functional unless explicitly
    disabled.
18. Optional RV32I processor emulation.
19. RISC-V → VFPGA control.
20. Quantitative performance comparison.

The final system should be described as:

> A software-defined, configurable digital logic fabric running on an
> ESP32-S3 N16R8, using bit-parallel and processor-level parallelism,
> with safe real physical I/O integration.

Never describe it as a physical FPGA.