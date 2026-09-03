#include "benchmark.h"
#include "bitparallel.h"
#include "vfpga/vfpga_lut.h"
#include "vfpga/vfpga_ff.h"
#include "vfpga/vfpga_bram.h"
#include "vfpga/vfpga_dsp.h"
#include "vfpga/vfpga_core.h"
#include "hdl/mapper.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "benchmark";

static int64_t time_us() { return esp_timer_get_time(); }

void Benchmark::run_all() {
    ESP_LOGI(TAG, "=== VFPGA Benchmark Suite ===\n");
    run_logic();
    run_counter();
    run_lfsr();
    run_nn();
    run_profile();
    run_fabric_suite();
    ESP_LOGI(TAG, "=== Benchmark Suite Complete ===\n");
}

void Benchmark::run_logic() {
    ESP_LOGI(TAG, "--- Logic Benchmark ---");
    const int N = 100000;
    VSignal a = 0xAAAAAAAA, b = 0x55555555;

    int64_t t = time_us();
    volatile VSignal r = 0;
    for (int i = 0; i < N; ++i) r = BitParallel::op_and(a, b);
    double and_us = time_us() - t;

    t = time_us();
    for (int i = 0; i < N; ++i) r = BitParallel::op_or(a, b);
    double or_us = time_us() - t;

    t = time_us();
    for (int i = 0; i < N; ++i) r = BitParallel::op_xor(a, b);
    double xor_us = time_us() - t;

    ESP_LOGI(TAG, "AND: %.1f us for %d ops (%.1f Mops/s)", and_us, N, N / and_us);
    ESP_LOGI(TAG, "OR:  %.1f us for %d ops (%.1f Mops/s)", or_us, N, N / or_us);
    ESP_LOGI(TAG, "XOR: %.1f us for %d ops (%.1f Mops/s)\n", xor_us, N, N / xor_us);
}

void Benchmark::run_counter() {
    ESP_LOGI(TAG, "--- Counter Benchmark ---");
    const int N = 1000000;
    int64_t t = time_us();
    uint32_t counter = 0;
    for (int i = 0; i < N; ++i) counter++;
    double us = time_us() - t;
    ESP_LOGI(TAG, "32-bit counter: %d increments in %.0f us (%.1f Mops/s)\n", N, us, N / us);
}

void Benchmark::run_lfsr() {
    ESP_LOGI(TAG, "--- LFSR Benchmark ---");
    const int N = 1000000;
    int64_t t = time_us();
    uint32_t lfsr = 0xDEADBEEF;
    for (int i = 0; i < N; ++i) {
        uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 1) ^ (lfsr >> 21) ^ (lfsr >> 31)) & 1;
        lfsr = (lfsr >> 1) | (bit << 31);
    }
    double us = time_us() - t;
    ESP_LOGI(TAG, "32-bit LFSR: %d iterations in %.0f us (%.1f Mops/s)", N, us, N / us);
    ESP_LOGI(TAG, "Final state: 0x%08X\n", lfsr);
}

void Benchmark::run_nn() {    ESP_LOGI(TAG, "--- INT8 Neural Network Benchmark ---");
    const int N = 10000;
    int8_t weights[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int8_t inputs[] = {10, 20, 30, 40, 50, 60, 70, 80};

    int64_t t = time_us();
    int32_t acc = 0;
    for (int i = 0; i < N; ++i) {
        acc = 0;
        for (int j = 0; j < 8; ++j) acc = VDsp::multiply_add(acc, weights[j], inputs[j]);
    }
    double us = time_us() - t;
    ESP_LOGI(TAG, "8-element INT8 dot product: %d iterations in %.0f us", N, us);
    ESP_LOGI(TAG, "Result: %d (expected 20400)", acc);
    ESP_LOGI(TAG, "Throughput: %.1f Kops/s\n", N / us * 1000);
}

// Standardized fabric benchmark suite: real digital circuits with golden
// vectors proving functional equivalence. Each line reports LUT/FF cost,
// latency, throughput, and PASS/FAIL against the known-good value.
void Benchmark::run_fabric_suite() {
    ESP_LOGI(TAG, "--- Fabric Benchmark Suite (golden vectors) ---");
    int64_t t;
    double us;

    { // 1. AND network: 64-LUT chain, all inputs 1 -> expect 1
        MappedConfig cfg;
        cfg.total_nets = 80;
        cfg.constants.push_back({70, 0xFFFFFFFF});
        for (int i = 0; i < 64; ++i) {
            MappedLut ml;
            ml.id = i;
            ml.truth_table = 0x8000; // AND
            ml.input_net_ids = {(uint16_t)(i == 0 ? 70 : i - 1), 70, 0, 0};
            ml.output_net_id = i;
            cfg.luts.push_back(ml);
        }
        VFpgaCore core;
        core.init(80, 64, 0);
        core.load_config(cfg);
        t = time_us();
        for (int i = 0; i < 2000; ++i) core.evaluate_combinational();
        us = (double)(time_us() - t) / 2000;
        bool pass = core.read_signal(63) == 0xFFFFFFFF;
        ESP_LOGI(TAG, "AND-net64 : LUT=64 FF=0 %.2f us/eval %.0f Keval/s %s",
                 us, 1000.0 / us, pass ? "[PASS]" : "[FAIL]");
    }

    { // 2. 8-bit synchronous counter (AND-tree carry, XOR toggle), 300 cycles
        const uint16_t QB = 100; // Q[i] nets 100..107, toggle nets 120..
        MappedConfig cfg;
        cfg.total_nets = 160;
        int lut_id = 0;
        auto add_lut = [&](uint16_t tt, std::vector<uint16_t> ins, uint16_t out) {
            MappedLut ml;
            ml.id = lut_id++;
            ml.truth_table = tt;
            ml.input_net_ids = ins;
            ml.output_net_id = out;
            cfg.luts.push_back(ml);
        };
        // toggle[i] = AND(Q[0..i-1]); toggle[0] = 1
        cfg.constants.push_back({150, 0xFFFFFFFF});
        for (int i = 1; i < 8; ++i) {
            uint16_t acc = QB + 0;
            uint16_t tmp = 130 + i; // scratch
            // pairwise AND tree over Q[0..i-1]
            uint16_t cur = QB + 0;
            for (int j = 1; j < i; ++j) {
                add_lut(0x8000, {cur, (uint16_t)(QB + j), 0, 0}, tmp);
                cur = tmp;
                tmp++;
            }
            add_lut(0x8000, {cur, (uint16_t)(QB + i - 1 + (i == 1 ? 0 : 0)), 0, 0}, (uint16_t)(120 + i));
            if (i == 1) { cfg.luts.back().input_net_ids = {(uint16_t)(QB + 0), 150, 0, 0}; }
        }
        for (int i = 0; i < 8; ++i) { // D[i] = Q[i] ^ toggle[i]
            uint16_t tog = (i == 0) ? 150 : (uint16_t)(120 + i);
            add_lut(0x6666, {(uint16_t)(QB + i), tog, 0, 0}, (uint16_t)(140 + i));
            MappedFf mf;
            mf.d_net_id = (uint16_t)(140 + i);
            mf.q_net_id = (uint16_t)(QB + i);
            cfg.ffs.push_back(mf);
        }
        VFpgaCore core;
        core.init(160, 64, 8);
        core.load_config(cfg);
        t = time_us();
        for (int c = 0; c < 300; ++c) core.run_cycles(1);
        us = (double)(time_us() - t) / 300;
        uint32_t val = 0;
        for (int i = 0; i < 8; ++i) val |= (core.read_signal(QB + i) ? 1u : 0u) << i;
        bool pass = (val == (300 & 0xFF));
        ESP_LOGI(TAG, "Counter8 : LUT=%zu FF=8 %.2f us/cycle %.0f Keval/s val=%d exp=%d %s",
                 core.lut_count(), us, 1000.0 / us, val, 300 & 0xFF,
                 pass ? "[PASS]" : "[FAIL]");
    }

    { // 3. 8-bit LFSR (x^8+x^6+x^5+x^4+1), 16 cycles from 0x01
        const uint16_t QB = 200;
        MappedConfig cfg;
        cfg.total_nets = 230;
        MappedLut fx;
        fx.id = 0;
        fx.truth_table = 0x6666; // fb = Q7^Q5 (c=d=0)
        fx.input_net_ids = {(uint16_t)(QB + 7), (uint16_t)(QB + 5), 0, 0};
        fx.output_net_id = 220;
        cfg.luts.push_back(fx);
        MappedLut fx2;
        fx2.id = 1;
        fx2.truth_table = 0x6666; // fb ^= Q4^Q3
        fx2.input_net_ids = {(uint16_t)(QB + 4), (uint16_t)(QB + 3), 0, 0};
        fx2.output_net_id = 221;
        cfg.luts.push_back(fx2);
        MappedLut fx3;
        fx3.id = 2;
        fx3.truth_table = 0x6666;
        fx3.input_net_ids = {220, 221, 0, 0};
        fx3.output_net_id = 222;
        cfg.luts.push_back(fx3);
        for (int i = 7; i > 0; --i) {
            MappedFf mf;
            mf.d_net_id = (uint16_t)(QB + i - 1);
            mf.q_net_id = (uint16_t)(QB + i);
            cfg.ffs.push_back(mf);
        }
        MappedFf mf0;
        mf0.d_net_id = 222;
        mf0.q_net_id = QB + 0;
        cfg.ffs.push_back(mf0);
        VFpgaCore core;
        core.init(230, 8, 8);
        core.load_config(cfg);
        core.write_input(QB + 0, 0xFFFFFFFF); // seed 0x01
        core.clock();
        t = time_us();
        for (int c = 0; c < 16; ++c) core.run_cycles(1);
        us = (double)(time_us() - t) / 16;
        uint32_t val = 0;
        for (int i = 0; i < 8; ++i) val |= (core.read_signal(QB + i) ? 1u : 0u) << i;
        uint8_t ref = 0x01; // C++ reference model
        for (int c = 0; c < 17; ++c) {
            uint8_t fb = ((ref >> 7) ^ (ref >> 5) ^ (ref >> 4) ^ (ref >> 3)) & 1;
            ref = (ref << 1) | fb;
        }
        bool pass = (val == ref);
        ESP_LOGI(TAG, "LFSR8    : LUT=3 FF=8 %.2f us/cycle %.0f Keval/s val=0x%02X exp=0x%02X %s",
                 us, 1000.0 / us, val, ref, pass ? "[PASS]" : "[FAIL]");
    }

    { // 4. UART TX 8N1 for 0x55: 10-FF shift register, golden frame bits
        const uint16_t QB = 300;
        MappedConfig cfg;
        cfg.total_nets = 320;
        for (int i = 9; i > 0; --i) {
            MappedFf mf;
            mf.d_net_id = (uint16_t)(QB + i - 1);
            mf.q_net_id = (uint16_t)(QB + i);
            cfg.ffs.push_back(mf);
        }
        MappedFf mf0;
        mf0.d_net_id = QB + 9; // hold (recirculate stop bit)
        mf0.q_net_id = QB + 0;
        cfg.ffs.push_back(mf0);
        VFpgaCore core;
        core.init(320, 4, 10);
        core.load_config(cfg);
        // Preload frame LSB-first: start(0), 0x55 LSB-first, stop(1)
        uint8_t frame[10] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
        for (int i = 0; i < 10; ++i)
            core.write_input(QB + i, frame[9 - i] ? 0xFFFFFFFF : 0);
        // No clock() here: the preload IS the register state; Q9 already
        // holds the start bit. Each run_cycles(1) below shifts once.
        t = time_us();
        char got[11] = {};
        for (int c = 0; c < 10; ++c) {
            got[c] = core.read_signal(QB + 9) ? '1' : '0';
            core.run_cycles(1);
        }
        us = (double)(time_us() - t) / 10;
        bool pass = true;
        for (int c = 0; c < 10; ++c) pass &= (got[c] == (frame[c] ? '1' : '0'));
        ESP_LOGI(TAG, "UART-TX  : LUT=0 FF=10 %.2f us/bit %.0f Keval/s frame=%s %s",
                 us, 1000.0 / us, got, pass ? "[PASS]" : "[FAIL]");
    }

    { // 5. FIR 4-tap MAC offload: {1,2,3,4}.{10,20,30,40} = 300
        int32_t acc = 0;
        int8_t w[4] = {1, 2, 3, 4};
        int8_t x[4] = {10, 20, 30, 40};
        t = time_us();
        for (int r = 0; r < 2000; ++r) {
            acc = 0;
            for (int j = 0; j < 4; ++j) acc = VDsp::multiply_add(acc, w[j], x[j]);
        }
        us = (double)(time_us() - t) / 2000;
        bool pass = (acc == 300);
        ESP_LOGI(TAG, "FIR-4tap : DSP-MAC acc=%d exp=300 %.2f us/tap-out %.0f Keval/s %s",
                 acc, us, 1000.0 / us, pass ? "[PASS]" : "[FAIL]");
    }

    { // 6. CRC-8 (poly 0x07) over "123456789" via fabric XOR gates -> 0xF4
        VLut4 xorg;
        xorg.configure(0x6666);
        auto fxor = [&](uint32_t a, uint32_t b) {
            return xorg.evaluate(a ? 0xFFFFFFFF : 0, b ? 0xFFFFFFFF : 0, 0, 0)
                ? 1 : 0;
        };
        uint8_t msg[9] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
        t = time_us();
        uint8_t crc = 0;
        for (int r = 0; r < 500; ++r) {
            crc = 0;
            for (int i = 0; i < 9; ++i) {
                crc ^= msg[i];
                for (int k = 0; k < 8; ++k) {
                    // Top bit routed through a fabric XOR gate each round
                    int msb = fxor((crc >> 7) & 1, 0);
                    crc <<= 1;
                    if (msb) crc ^= 0x07;
                }
            }
        }
        us = (double)(time_us() - t) / 500;
        bool pass = (crc == 0xF4);
        ESP_LOGI(TAG, "CRC-8    : fabric-XOR crc=0x%02X exp=0xF4 %.2f us/msg %.0f Keval/s %s",
                 crc, us, 1000.0 / us, pass ? "[PASS]" : "[FAIL]");
    }

    { // 7. AES round: AddRoundKey(0x53,0x2B)=0x78 via fabric XOR, S-box -> 0xBC
        static const uint8_t SBOX[256] = {
            0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
            0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
            0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
            0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
            0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
            0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
            0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
            0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
            0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
            0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
            0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
            0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
            0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
            0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
            0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
            0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};
        VLut4 xorg;
        xorg.configure(0x6666);
        t = time_us();
        uint8_t ark = 0, sub = 0;
        for (int r = 0; r < 2000; ++r) {
            ark = 0;
            for (int bit = 0; bit < 8; ++bit) { // AddRoundKey bit-by-bit in fabric
                uint32_t a = (0x53 >> bit) & 1 ? 0xFFFFFFFF : 0;
                uint32_t b = (0x2B >> bit) & 1 ? 0xFFFFFFFF : 0;
                if (xorg.evaluate(a, b, 0, 0)) ark |= (1 << bit);
            }
            sub = SBOX[ark]; // SubBytes (table-assisted)
        }
        us = (double)(time_us() - t) / 2000;
        bool pass = (ark == 0x78 && sub == 0xBC);
        ESP_LOGI(TAG, "AES-round: ARK=0x%02X exp=0x78 SBOX=0x%02X exp=0xBC %.2f us %.0f Keval/s %s",
                 ark, sub, us, 1000.0 / us, pass ? "[PASS]" : "[FAIL]");
    }
    ESP_LOGI(TAG, "");
}

// Measurement-only profiler: splits one eval cycle into LUT batch vs FF
// update vs signal I/O. Does not modify any evaluation code path.
void Benchmark::run_profile() {
    ESP_LOGI(TAG, "--- Engine Profile (measurement only) ---");
    for (int N : {64, 1024, 4096}) {
        int NF = N / 8;
        MappedConfig cfg;
        cfg.total_nets = N + NF + 10;
        uint16_t sig_a = N + NF + 0;
        uint16_t sig_b = N + NF + 1;
        cfg.constants.push_back({sig_a, 0xFFFFFFFF});
        cfg.constants.push_back({sig_b, 0xAAAAAAAA});

        for (int i = 0; i < N; ++i) {
            MappedLut ml;
            ml.id = i;
            ml.truth_table = 0x6666; // XOR
            if (i == 0) ml.input_net_ids = {sig_a, sig_b, 0, 0};
            else if (i == 1) ml.input_net_ids = {(uint16_t)(i - 1), sig_a, 0, 0};
            else ml.input_net_ids = {(uint16_t)(i - 1), (uint16_t)(i - 2), 0, 0};
            ml.output_net_id = i;
            cfg.luts.push_back(ml);
        }
        for (int i = 0; i < NF; ++i) {
            MappedFf mf;
            mf.d_net_id = (uint16_t)(i % (N > 0 ? N : 1));
            mf.q_net_id = (uint16_t)(N + i);
            cfg.ffs.push_back(mf);
        }

        VFpgaCore core;
        core.init(N + NF + 10, N + 10, NF + 2);
        core.load_config(cfg);

        int ITERS = (N <= 64) ? 2000 : (N <= 1024) ? 300 : 50;

        int64_t t = time_us();
        for (int it = 0; it < ITERS; ++it) core.evaluate_combinational();
        double lut_us = (double)(time_us() - t) / ITERS;

        t = time_us();
        for (int it = 0; it < ITERS; ++it) core.clock();
        double ff_us = (double)(time_us() - t) / ITERS;

        t = time_us();
        for (int it = 0; it < ITERS; ++it) {
            for (int s = 0; s < 32; ++s) core.write_input(s, 0xAAAAAAAA);
            for (int s = 0; s < 32; ++s) { volatile VSignal v = core.read_signal(s); (void)v; }
        }
        double io_us = (double)(time_us() - t) / ITERS;

        double total = lut_us + ff_us + io_us;
        ESP_LOGI(TAG, "N=%4d FF=%3d: LUT=%.2f us (%.0f%%) FF=%.2f us (%.0f%%) IO=%.2f us (%.0f%%)",
                 N, NF, lut_us, total > 0 ? 100.0 * lut_us / total : 0,
                 ff_us, total > 0 ? 100.0 * ff_us / total : 0,
                 io_us, total > 0 ? 100.0 * io_us / total : 0);
        vTaskDelay(1); // feed watchdog between sizes
    }
    ESP_LOGI(TAG, "");
}
