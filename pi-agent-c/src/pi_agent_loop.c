#include "pi_agent.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *id;
    char *name;
    char *args;
} pending_tool_call_t;

typedef struct {
    pending_tool_call_t *calls;
    size_t count;
} tool_call_queue_t;

typedef struct {
    pi_ai_callback_t original_callback;
    void *user_data;
    tool_call_queue_t *queue;
} agent_callback_ctx_t;

static void agent_internal_callback(pi_ai_event_t *event, void *user_data) {
    agent_callback_ctx_t *ctx = (agent_callback_ctx_t *)user_data;

    if (event->type == PI_AI_EVENT_TOOLCALL_END) {
        // Collect tool call
        ctx->queue->calls = realloc(ctx->queue->calls, sizeof(pending_tool_call_t) * (ctx->queue->count + 1));
        pending_tool_call_t *call = &ctx->queue->calls[ctx->queue->count];
        // We need to extract ID and Name from the event or raw data.
        // For simplicity in this implementation, we assume we need to parse the full content or have it in raw data.
        // Let's assume the event->full_content is the JSON of arguments, but we need ID and Name too.
        // In pi_ai_openai.c we have it. Let's make sure it's accessible.

        call->id = strdup(event->tool_call_id ? event->tool_call_id : "unknown");
        call->name = strdup(event->tool_call_name ? event->tool_call_name : "unknown");
        call->args = strdup(event->full_content);
        ctx->queue->count++;
    }

    // Forward to user callback
    if (ctx->original_callback) {
        ctx->original_callback(event, ctx->user_data);
    }
}

// Helper to find agent tool
static pi_agent_tool_t* find_agent_tool(pi_agent_t *agent, const char *name) {
    for (size_t i = 0; i < agent->agent_tool_count; i++) {
        if (strcmp(agent->agent_tools[i].info.name, name) == 0) {
            return &agent->agent_tools[i];
        }
    }
    return NULL;
}

int pi_agent_run(pi_agent_t *agent, const char *user_prompt, pi_ai_callback_t callback) {
    if (user_prompt) {
        agent->context.messages = realloc(agent->context.messages, sizeof(pi_ai_message_t) * (agent->context.message_count + 1));
        pi_ai_message_t *msg = &agent->context.messages[agent->context.message_count];
        memset(msg, 0, sizeof(pi_ai_message_t));
        msg->role = strdup("user");
        pi_ai_message_add_text(msg, user_prompt);
        agent->context.message_count++;
    }

    for (int iter = 0; iter < agent->max_iterations; iter++) {
        tool_call_queue_t queue = {0};
        agent_callback_ctx_t ctx = {callback, agent->user_data, &queue};

        int res = pi_ai_openai_stream(&agent->options, &agent->context, agent_internal_callback, &ctx);
        if (res != 0) return res;

        if (queue.count == 0) break; // Done

        // Execute tools
        for (size_t i = 0; i < queue.count; i++) {
            pi_agent_tool_t *atool = find_agent_tool(agent, queue.calls[i].name);
            pi_agent_tool_result_t result;
            if (atool) {
                cJSON *args = cJSON_Parse(queue.calls[i].args);
                result = atool->proc(queue.calls[i].id, args, agent->user_data);
                cJSON_Delete(args);
            } else {
                result.is_error = true;
                result.content = cJSON_CreateString("Tool not found");
            }

            // Append Tool Result to context
            agent->context.messages = realloc(agent->context.messages, sizeof(pi_ai_message_t) * (agent->context.message_count + 1));
            pi_ai_message_t *tm = &agent->context.messages[agent->context.message_count];
            memset(tm, 0, sizeof(pi_ai_message_t));
            tm->role = strdup("toolResult");
            tm->tool_call_id = strdup(queue.calls[i].id);
            tm->tool_name = strdup(queue.calls[i].name);
            tm->is_error = result.is_error;

            char *res_str = cJSON_PrintUnformatted(result.content);
            pi_ai_message_add_text(tm, res_str);
            free(res_str);
            cJSON_Delete(result.content);
            if (result.details) cJSON_Delete(result.details);

            agent->context.message_count++;

            free(queue.calls[i].id);
            free(queue.calls[i].name);
            free(queue.calls[i].args);
        }
        free(queue.calls);
    }

    return 0;
}
