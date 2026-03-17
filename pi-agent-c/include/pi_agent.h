#ifndef PI_AGENT_H
#define PI_AGENT_H

#include "pi_ai.h"
#include "pi_ai_openai.h"
#include <cJSON.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    cJSON *content; // Can be text or structured data
    cJSON *details; // Additional metadata
    bool is_error;
} pi_agent_tool_result_t;

typedef struct {
    bool block;
    char *reason;
} pi_agent_before_tool_call_result_t;

typedef struct {
    cJSON *content_override;
    bool is_error_override;
    bool use_is_error_override;
} pi_agent_after_tool_call_result_t;

/** Context for hooks */
typedef struct {
    const char *tool_call_id;
    const char *tool_name;
    const cJSON *arguments;
    void *user_data;
} pi_agent_hook_context_t;

typedef pi_agent_before_tool_call_result_t (*pi_agent_before_tool_call_t)(pi_agent_hook_context_t *ctx);
typedef pi_agent_after_tool_call_result_t (*pi_agent_after_tool_call_t)(pi_agent_hook_context_t *ctx, pi_agent_tool_result_t result);

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

typedef enum {
    PI_AGENT_TOOL_EXECUTION_SEQUENTIAL,
    PI_AGENT_TOOL_EXECUTION_PARALLEL
} pi_agent_tool_execution_mode_t;

typedef struct pi_agent_msg_node {
    pi_ai_message_t *message;
    struct pi_agent_msg_node *next;
} pi_agent_msg_node_t;

typedef struct {
    pi_agent_msg_node_t *head;
    pi_agent_msg_node_t *tail;
    size_t count;
} pi_agent_msg_queue_t;

typedef struct {
    pi_ai_context_t context;
    pi_ai_openai_options_t options;
    pi_agent_tool_t *agent_tools;
    size_t agent_tool_count;
    pi_agent_tool_execution_mode_t tool_execution_mode;

    // Hooks
    pi_agent_before_tool_call_t before_tool_call;
    pi_agent_after_tool_call_t after_tool_call;

    // Retry configuration
    int max_retries_on_error;
    int retry_delay_ms;

    // Async features
    pthread_mutex_t mutex;
    pi_agent_msg_queue_t steering_queue;
    pi_agent_msg_queue_t follow_up_queue;
    bool abort_requested;

    int max_iterations;
    void *user_data;
} pi_agent_t;

// Agent Management
pi_agent_t* pi_agent_create(const pi_ai_openai_options_t *options);
void pi_agent_free(pi_agent_t *agent);

// Tool Registration
void pi_agent_register_tool(pi_agent_t *agent, const char *name, const char *description, const char *parameters_json, pi_agent_tool_proc_t proc);

// Steering & Follow-up
void pi_agent_steer(pi_agent_t *agent, const char *text);
void pi_agent_follow_up(pi_agent_t *agent, const char *text);

// Execution
int pi_agent_run(pi_agent_t *agent, const char *user_prompt, pi_ai_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // PI_AGENT_H
