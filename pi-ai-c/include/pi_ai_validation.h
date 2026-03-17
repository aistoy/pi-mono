#ifndef PI_AI_VALIDATION_H
#define PI_AI_VALIDATION_H

#include "pi_ai.h"
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool valid;
    char *error_message;
} pi_ai_validation_result_t;

/**
 * Validates tool arguments against the tool's parameter schema.
 * Supports checking required fields and basic types.
 */
pi_ai_validation_result_t pi_ai_validate_tool_call(const pi_ai_tool_t *tool, const char *arguments_json);

void pi_ai_free_validation_result(pi_ai_validation_result_t result);

#ifdef __cplusplus
}
#endif

#endif // PI_AI_VALIDATION_H
