#include "test_framework.h"
#include "engine/bitparallel.h"
#include "esp_log.h"

static const char *TAG = "test_bp";

void test_bitparallel() {
    ESP_LOGI(TAG, "=== M1: Bit-Parallel Engine Tests ===");
    int pass = 0;
    int fail = 0;

    VSignal a = 0xAAAAAAAA;
    VSignal b = 0x55555555;

    // AND
    VSignal expected_and = a & b;
    VSignal result_and = BitParallel::op_and(a, b);
    if (result_and == expected_and) { ESP_LOGI(TAG, "[PASS] AND"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] AND: got 0x%08X expected 0x%08X", result_and, expected_and); fail++; }

    // OR
    VSignal expected_or = a | b;
    VSignal result_or = BitParallel::op_or(a, b);
    if (result_or == expected_or) { ESP_LOGI(TAG, "[PASS] OR"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] OR: got 0x%08X expected 0x%08X", result_or, expected_or); fail++; }

    // XOR
    VSignal expected_xor = a ^ b;
    VSignal result_xor = BitParallel::op_xor(a, b);
    if (result_xor == expected_xor) { ESP_LOGI(TAG, "[PASS] XOR"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] XOR: got 0x%08X expected 0x%08X", result_xor, expected_xor); fail++; }

    // NOT
    VSignal expected_not = ~a;
    VSignal result_not = BitParallel::op_not(a);
    if (result_not == expected_not) { ESP_LOGI(TAG, "[PASS] NOT"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] NOT: got 0x%08X expected 0x%08X", result_not, expected_not); fail++; }

    // NAND
    VSignal expected_nand = ~(a & b);
    VSignal result_nand = BitParallel::op_nand(a, b);
    if (result_nand == expected_nand) { ESP_LOGI(TAG, "[PASS] NAND"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] NAND: got 0x%08X expected 0x%08X", result_nand, expected_nand); fail++; }

    // NOR
    VSignal expected_nor = ~(a | b);
    VSignal result_nor = BitParallel::op_nor(a, b);
    if (result_nor == expected_nor) { ESP_LOGI(TAG, "[PASS] NOR"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] NOR: got 0x%08X expected 0x%08X", result_nor, expected_nor); fail++; }

    // XNOR
    VSignal expected_xnor = ~(a ^ b);
    VSignal result_xnor = BitParallel::op_xnor(a, b);
    if (result_xnor == expected_xnor) { ESP_LOGI(TAG, "[PASS] XNOR"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] XNOR: got 0x%08X expected 0x%08X", result_xnor, expected_xnor); fail++; }

    // MUX (sel=0 -> a, sel=1 -> b)
    VSignal result_mux0 = BitParallel::op_mux(a, b, 0x00000000);
    VSignal result_mux1 = BitParallel::op_mux(a, b, 0xFFFFFFFF);
    if (result_mux0 == a && result_mux1 == b) { ESP_LOGI(TAG, "[PASS] MUX"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] MUX: sel=0 got 0x%08X, sel=1 got 0x%08X", result_mux0, result_mux1); fail++; }

    // Test all-ones AND all-ones
    VSignal ones_and = BitParallel::op_and(0xFFFFFFFF, 0xFFFFFFFF);
    if (ones_and == 0xFFFFFFFF) { ESP_LOGI(TAG, "[PASS] ALL_ONES AND"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] ALL_ONES AND"); fail++; }

    // Test alternating bits
    VSignal alt_a = 0xAAAAAAAA;
    VSignal alt_b = 0x55555555;
    VSignal alt_and = BitParallel::op_and(alt_a, alt_b);
    if (alt_and == 0x00000000) { ESP_LOGI(TAG, "[PASS] Alternating AND (should be 0)"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Alternating AND"); fail++; }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Bit-Parallel: %d passed, %d failed", pass, fail);
    ESP_LOGI(TAG, "");
}
