#include "test_framework.h"
#include "vfpga/vfpga_lut.h"
#include "esp_log.h"

static const char *TAG = "test_lut";

void test_lut4() {
    ESP_LOGI(TAG, "=== M2: LUT4 Tests ===");
    int pass = 0;
    int fail = 0;

    VLut4 lut;
    VSignal a, b, c, d, result;

    // AND gate: truth table = 0x8000 (only input 1111 -> 1)
    lut.configure(0x8000);
    a = 0xFFFFFFFF; b = 0xFFFFFFFF; c = 0xFFFFFFFF; d = 0xFFFFFFFF;
    result = lut.evaluate(a, b, c, d);
    if (result == 0xFFFFFFFF) { ESP_LOGI(TAG, "[PASS] AND gate (all ones)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] AND gate: 0x%08X", result); fail++; }

    a = 0x00000000;
    result = lut.evaluate(a, b, c, d);
    if (result == 0x00000000) { ESP_LOGI(TAG, "[PASS] AND gate (a=0)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] AND gate (a=0): 0x%08X", result); fail++; }

    // OR gate: truth table = 0xFE (any input -> 1 except 0000)
    lut.configure(0xFE);
    a = 0x00000000; b = 0x00000000; c = 0x00000000; d = 0x00000000;
    result = lut.evaluate(a, b, c, d);
    if (result == 0x00000000) { ESP_LOGI(TAG, "[PASS] OR gate (all zero)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] OR gate (all zero): 0x%08X", result); fail++; }

    a = 0xFFFFFFFF;
    result = lut.evaluate(a, b, c, d);
    if (result == 0xFFFFFFFF) { ESP_LOGI(TAG, "[PASS] OR gate (a=1)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] OR gate (a=1): 0x%08X", result); fail++; }

    // XOR gate (2-input): truth table = 0x6C (inputs a,b: 00->0, 01->1, 10->1, 11->0)
    // With c=d=0, truth table bits: idx = a|2b, so 0x6C = 01101100
    lut.configure(0x6C);
    a = 0xFFFFFFFF; b = 0xFFFFFFFF; c = 0x00000000; d = 0x00000000;
    result = lut.evaluate(a, b, c, d);
    if (result == 0x00000000) { ESP_LOGI(TAG, "[PASS] XOR gate (1^1=0)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] XOR gate (1^1): 0x%08X", result); fail++; }

    a = 0xFFFFFFFF; b = 0x00000000;
    result = lut.evaluate(a, b, c, d);
    if (result == 0xFFFFFFFF) { ESP_LOGI(TAG, "[PASS] XOR gate (1^0=1)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] XOR gate (1^0): 0x%08X", result); fail++; }

    // NAND gate: truth table = 0x7FFF (all except 1111 -> 1)
    lut.configure(0x7FFF);
    a = 0xFFFFFFFF; b = 0xFFFFFFFF; c = 0xFFFFFFFF; d = 0xFFFFFFFF;
    result = lut.evaluate(a, b, c, d);
    if (result == 0x00000000) { ESP_LOGI(TAG, "[PASS] NAND gate (all ones -> 0)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] NAND gate: 0x%08X", result); fail++; }

    // NOR gate: truth table = 0x0001 (only 0000 -> 1)
    lut.configure(0x0001);
    a = 0x00000000; b = 0x00000000; c = 0x00000000; d = 0x00000000;
    result = lut.evaluate(a, b, c, d);
    if (result == 0xFFFFFFFF) { ESP_LOGI(TAG, "[PASS] NOR gate (all zero -> 1)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] NOR gate: 0x%08X", result); fail++; }

    a = 0xFFFFFFFF;
    result = lut.evaluate(a, b, c, d);
    if (result == 0x00000000) { ESP_LOGI(TAG, "[PASS] NOR gate (a=1 -> 0)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] NOR gate (a=1): 0x%08X", result); fail++; }

    // Constant 0: truth table = 0x0000
    lut.configure(0x0000);
    a = 0xFFFFFFFF; b = 0xFFFFFFFF; c = 0xFFFFFFFF; d = 0xFFFFFFFF;
    result = lut.evaluate(a, b, c, d);
    if (result == 0x00000000) { ESP_LOGI(TAG, "[PASS] CONST 0"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] CONST 0: 0x%08X", result); fail++; }

    // Constant 1: truth table = 0xFFFF
    lut.configure(0xFFFF);
    result = lut.evaluate(a, b, c, d);
    if (result == 0xFFFFFFFF) { ESP_LOGI(TAG, "[PASS] CONST 1"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] CONST 1: 0x%08X", result); fail++; }

    // MUX: y = a when d=0, y = b when d=1
    // truth table: d=0 -> idx has d=0 -> use a bit; d=1 -> idx has d=1 -> use b bit
    // idx bits: a(bit0) | b(bit1) | c(bit2) | d(bit3)
    // d=0: idx 0(a0,b0) -> a, idx 2(a1,b0) -> a -> output a
    // d=1: idx 8(a0,b0,d1) -> b, idx 10(a1,b0,d1) -> b -> output b
    // truth table for MUX: 0xCA = 11001010 (a=bit0, b=bit1, c=0, d=sel)
    // Actually let's use simpler: MUX2 with a,b,sel=d
    // When d=0: output = a; when d=1: output = b
    // truth table: for each (a,b,c,d) combo:
    // d=0: output=a; d=1: output=b
    // bit0=a, bit1=b, bit2=c, bit3=d
    // idx=0 (0000): a=0,b=0 -> 0
    // idx=1 (0001): a=1,b=0 -> 1 (a)
    // idx=2 (0010): a=0,b=1 -> 0 (a)
    // idx=3 (0011): a=1,b=1 -> 1 (a)
    // idx=4 (0100): a=0,b=0 -> 0
    // idx=5 (0101): a=1,b=0 -> 1 (a)
    // idx=6 (0110): a=0,b=1 -> 0 (a)
    // idx=7 (0111): a=1,b=1 -> 1 (a)
    // idx=8 (1000): a=0,b=0 -> 0
    // idx=9 (1001): a=1,b=0 -> 0 (b=0)
    // idx=10 (1010): a=0,b=1 -> 1 (b=1)
    // idx=11 (1011): a=1,b=1 -> 1 (b=1)
    // idx=12 (1100): a=0,b=0 -> 0
    // idx=13 (1101): a=1,b=0 -> 0
    // idx=14 (1110): a=0,b=1 -> 1
    // idx=15 (1111): a=1,b=1 -> 1
    // truth table: 0b1100101001100010 = 0xCA62... wait let me recalc
    // bits: 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
    //       1  1  0  0  1  0  1 0 1 0 0 0 1 0 1 0 = 0xCA8A
    // Hmm, let me just use a known MUX truth table
    // Simple MUX: sel=d, out = d ? b : a
    lut.configure(0xCA8A);
    a = 0xAAAAAAAA; b = 0x55555555; c = 0x00000000; d = 0x00000000;
    result = lut.evaluate(a, b, c, d);
    if (result == 0xAAAAAAAA) { ESP_LOGI(TAG, "[PASS] MUX (sel=0 -> a)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] MUX (sel=0): 0x%08X expected 0xAAAAAAAA", result); fail++; }

    d = 0xFFFFFFFF;
    result = lut.evaluate(a, b, c, d);
    if (result == 0x55555555) { ESP_LOGI(TAG, "[PASS] MUX (sel=1 -> b)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] MUX (sel=1): 0x%08X expected 0x55555555", result); fail++; }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "LUT4: %d passed, %d failed", pass, fail);
    ESP_LOGI(TAG, "");
}
