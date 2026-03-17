#include "pi_agent.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

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

typedef struct {
    pi_agent_t *agent;
    pi_agent_tool_t *atool;
    const char *id;
    const char *name;
    const char *args_json;
    pi_agent_tool_result_t result;
} tool_thread_arg_t;

static pi_agent_tool_result_t create_error_result(const char *msg) {
    pi_agent_tool_result_t res = {0};
    res.content = cJSON_CreateObject();
    cJSON_AddStringToObject(res.content, "error", msg);
    res.is_error = true;
    return res;
}

static pi_agent_tool_result_t execute_single_tool(pi_agent_t *agent, pi_agent_tool_t *atool, const char *id, const char *name, const char *args_json) {
    cJSON *args = cJSON_Parse(args_json);
    pi_agent_hook_context_t hook_ctx = {id, name, args, agent->user_data};
    pi_agent_tool_result_t result;

    // Before hook
    if (agent->before_tool_call) {
        pi_agent_before_tool_call_result_t before = agent->before_tool_call(&hook_ctx);
        if (before.block) {
            result = create_error_result(before.reason ? before.reason : "Blocked by hook");
            cJSON_Delete(args);
            return result;
        }
    }

    if (atool) {
        result = atool->proc(id, args, agent->user_data);
    } else {
        result = create_error_result("Tool not found");
    }

    // After hook
    if (agent->after_tool_call) {
        pi_agent_after_tool_call_result_t after = agent->after_tool_call(&hook_ctx, result);
        if (after.content_override) {
            cJSON_Delete(result.content);
            result.content = after.content_override;
        }
        if (after.use_is_error_override) {
            result.is_error = after.is_error_override;
        }
    }

    cJSON_Delete(args);
    return result;
}

static void* tool_thread_func(void *arg) {
    tool_thread_arg_t *targ = (tool_thread_arg_t *)arg;
    targ->result = execute_single_tool(targ->agent, targ->atool, targ->id, targ->name, targ->args_json);
    return NULL;
}

static void agent_internal_callback(pi_ai_event_t *event, void *user_data) {
    agent_callback_ctx_t *ctx = (agent_callback_ctx_t *)user_data;

    if (event->type == PI_AI_EVENT_TOOLCALL_END) {
        ctx->queue->calls = realloc(ctx->queue->calls, sizeof(pending_tool_call_t) * (ctx->queue->count + 1));
        pending_tool_call_t *call = &ctx->queue->calls[ctx->queue->count];
        call->id = strdup(event->tool_call_id ? event->tool_call_id : "unknown");
        call->name = strdup(event->tool_call_name ? event->tool_call_name : "unknown");
        call->args = strdup(event->full_content);
        ctx->queue->count++;
    }

    if (ctx->original_callback) {
        ctx->original_callback(event, ctx->user_data);
    }
}

static pi_agent_tool_t* find_agent_tool(pi_agent_t *agent, const char *name) {
    for (size_t i = 0; i < agent->agent_tool_count; i++) {
        if (strcmp(agent->agent_tools[i].info.name, name) == 0) {
            return &agent->agent_tools[i];
        }
    }
    return NULL;
}

static void append_tool_result(pi_agent_t *agent, const char *id, const char *name, pi_agent_tool_result_t result) {
    agent->context.messages = realloc(agent->context.messages, sizeof(pi_ai_message_t) * (agent->context.message_count + 1));
    pi_ai_message_t *tm = &agent->context.messages[agent->context.message_count];
    memset(tm, 0, sizeof(pi_ai_message_t));
    tm->role = strdup("toolResult");
    tm->tool_call_id = strdup(id);
    tm->tool_name = strdup(name);
    tm->is_error = result.is_error;

    char *res_str = cJSON_PrintUnformatted(result.content);
    pi_ai_message_add_text(tm, res_str);
    free(res_str);
    cJSON_Delete(result.content);
    if (result.details) cJSON_Delete(result.details);

    agent->context.message_count++;
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

        int res = -1;
        int retries = 0;
        do {
            res = pi_ai_openai_stream(&agent->options, &agent->context, agent_internal_callback, &ctx);
            if (res != 0 && retries < agent->max_retries_on_error) {
                retries++;
                if (agent->retry_delay_ms > 0) usleep(agent->retry_delay_ms * 1000);
                continue;
            }
            break;
        } while (1);

        if (res != 0) return res;
        if (queue.count == 0) break;

        if (agent->tool_execution_mode == PI_AGENT_TOOL_EXECUTION_PARALLEL) {
            pthread_t *threads = malloc(sizeof(pthread_t) * queue.count);
            tool_thread_arg_t *args = malloc(sizeof(tool_thread_arg_t) * queue.count);

            for (size_t i = 0; i < queue.count; i++) {
                args[i].agent = agent;
                args[i].atool = find_agent_tool(agent, queue.calls[i].name);
                args[i].id = queue.calls[i].id;
                args[i].name = queue.calls[i].name;
                args[i].args_json = queue.calls[i].args;
                pthread_create(&threads[i], NULL, tool_thread_func, &args[i]);
            }

            for (size_t i = 0; i < queue.count; i++) {
                pthread_join(threads[i], NULL);
                append_tool_result(agent, queue.calls[i].id, queue.calls[i].name, args[i].result);
            }

            free(threads);
            free(args);
        } else {
            for (size_t i = 0; i < queue.count; i++) {
                pi_agent_tool_t *atool = find_agent_tool(agent, queue.calls[i].name);
                pi_agent_tool_result_t result = execute_single_tool(agent, atool, queue.calls[i].id, queue.calls[i].name, queue.calls[i].args);
                append_tool_result(agent, queue.calls[i].id, queue.calls[i].name, result);
            }
        }

        for (size_t i = 0; i < queue.count; i++) {
            free(queue.calls[i].id);
            free(queue.calls[i].name);
            free(queue.calls[i].args);
        }
        free(queue.calls);
    }

    return 0;
}
