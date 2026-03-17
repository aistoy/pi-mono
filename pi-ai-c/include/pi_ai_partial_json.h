#ifndef PI_AI_PARTIAL_JSON_H
#define PI_AI_PARTIAL_JSON_H

#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Attempts to parse a partial JSON string.
 * It will try to "close" the JSON string by appending necessary brackets/quotes
 * to make it a valid JSON that cJSON can parse.
 */
cJSON* pi_ai_parse_partial_json(const char *partial_json);

#ifdef __cplusplus
}
#endif

#endif // PI_AI_PARTIAL_JSON_H
