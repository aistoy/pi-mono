#ifndef PI_AGENT_H
#define PI_AGENT_H

#include "pi_ai.h"
#include "pi_ai_openai.h"
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    cJSON *content; // Can be text or structured data
    cJSON *details; // Additional metadata
    bool is_error;
} pi_agent_tool_result_t;

/**
 * Tool execution function pointer.
 * Should return a pi_agent_tool_result_t.
 */
typedef pi_agent_tool_result_t (*pi_agent_tool_proc_t)(
    const char *tool_call_id,
    const cJSON *arguments,
    void *user_data
);

typedef struct {
    pi_ai_tool_t info;
    pi_agent_tool_proc_t proc;
} pi_agent_tool_t;

typedef struct {
    pi_ai_context_t context;
    pi_ai_openai_options_t options;
    pi_agent_tool_t *agent_tools;
    size_t agent_tool_count;
    int max_iterations;
    void *user_data;
} pi_agent_t;

// Agent Management
pi_agent_t* pi_agent_create(const pi_ai_openai_options_t *options);
void pi_agent_free(pi_agent_t *agent);

// Tool Registration
void pi_agent_register_tool(pi_agent_t *agent, const char *name, const char *description, const char *parameters_json, pi_agent_tool_proc_t proc);

// Execution
int pi_agent_run(pi_agent_t *agent, const char *user_prompt, pi_ai_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // PI_AGENT_H
