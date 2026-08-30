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

    // XOR gate (2-input): with c=d=0, idx = a|2b
    // 00->0, 01->1, 10->1, 11->0 -> bits 1,2 set = 0x06
    lut.configure(0x06);
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

    // MUX: sel=d, out = d ? b : a
    // idx bits: a(bit0) | b(bit1) | c(bit2) | d(bit3)
    // d=0: output=a; d=1: output=b
    // bits set: 1,3,5,7 (d=0, output=a) + 10,11,14,15 (d=1, output=b) = 0xCCAA
    lut.configure(0xCCAA);
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
