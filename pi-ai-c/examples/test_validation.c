#include "pi_ai_validation.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

void test_validation() {
    pi_ai_tool_t tool;
    tool.name = strdup("test_tool");
    tool.description = strdup("A test tool");
    tool.parameters_json = strdup("{\"type\":\"object\",\"properties\":{\"age\":{\"type\":\"number\"},\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}");

    // Test 1: Valid
    pi_ai_validation_result_t res1 = pi_ai_validate_tool_call(&tool, "{\"name\":\"Jules\",\"age\":25}");
    assert(res1.valid == true);
    pi_ai_free_validation_result(res1);

    // Test 2: Missing required
    pi_ai_validation_result_t res2 = pi_ai_validate_tool_call(&tool, "{\"age\":25}");
    assert(res2.valid == false);
    printf("Expected error: %s\n", res2.error_message);
    pi_ai_free_validation_result(res2);

    // Test 3: Wrong type
    pi_ai_validation_result_t res3 = pi_ai_validate_tool_call(&tool, "{\"name\":123}");
    assert(res3.valid == false);
    printf("Expected error: %s\n", res3.error_message);
    pi_ai_free_validation_result(res3);

    pi_ai_free_tool(&tool);
    printf("Validation tests passed!\n");
}

int main() {
    test_validation();
    return 0;
}
