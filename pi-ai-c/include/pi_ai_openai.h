#ifndef PI_AI_OPENAI_H
#define PI_AI_OPENAI_H

#include "pi_ai.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *api_base_url;
    const char *api_key;
    const char *model_id;
    double temperature;
    int max_tokens;
} pi_ai_openai_options_t;

/**
 * Executes a streaming completion using an OpenAI-compatible API.
 * This is a blocking call that runs the event loop.
 */
int pi_ai_openai_stream(
    const pi_ai_openai_options_t *options,
    const pi_ai_context_t *context,
    pi_ai_callback_t callback,
    void *user_data
);

#ifdef __cplusplus
}
#endif

#endif // PI_AI_OPENAI_H
