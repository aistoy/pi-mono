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
    pi_agent_t *agent;
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

static void append_msg_to_context(pi_agent_t *agent, pi_ai_message_t *msg) {
    agent->context.messages = realloc(agent->context.messages, sizeof(pi_ai_message_t) * (agent->context.message_count + 1));
    pi_ai_message_t *target = &agent->context.messages[agent->context.message_count];

    // Manual deep copy of message to context
    target->role = strdup(msg->role);
    target->tool_call_id = msg->tool_call_id ? strdup(msg->tool_call_id) : NULL;
    target->tool_name = msg->tool_name ? strdup(msg->tool_name) : NULL;
    target->is_error = msg->is_error;
    target->content_count = 0;
    target->contents = NULL;

    for (size_t i = 0; i < msg->content_count; i++) {
        if (msg->contents[i].type == PI_AI_CONTENT_TEXT) {
            pi_ai_message_add_text(target, msg->contents[i].data.text);
        }
        // ... extend for other types if needed
    }
    agent->context.message_count++;
}

static void consume_queue_to_context(pi_agent_t *agent, pi_agent_msg_queue_t *queue) {
    pthread_mutex_lock(&agent->mutex);
    pi_agent_msg_node_t *curr = queue->head;
    while (curr) {
        append_msg_to_context(agent, curr->message);
        pi_agent_msg_node_t *next = curr->next;
        pi_ai_free_message(curr->message);
        free(curr->message);
        free(curr);
        curr = next;
    }
    queue->head = queue->tail = NULL;
    queue->count = 0;
    pthread_mutex_unlock(&agent->mutex);
}

static void append_tool_result(pi_agent_t *agent, const char *id, const char *name, pi_agent_tool_result_t result) {
    pi_ai_message_t msg = {0};
    msg.role = "toolResult";
    msg.tool_call_id = (char*)id;
    msg.tool_name = (char*)name;
    msg.is_error = result.is_error;

    char *res_str = cJSON_PrintUnformatted(result.content);
    pi_ai_message_add_text(&msg, res_str);
    append_msg_to_context(agent, &msg);

    free(res_str);
    pi_ai_free_content(&msg.contents[0]);
    free(msg.contents);
    cJSON_Delete(result.content);
    if (result.details) cJSON_Delete(result.details);
}

int pi_agent_run(pi_agent_t *agent, const char *user_prompt, pi_ai_callback_t callback) {
    if (user_prompt) {
        pi_ai_message_t msg = {0};
        msg.role = "user";
        pi_ai_message_add_text(&msg, user_prompt);
        append_msg_to_context(agent, &msg);
        pi_ai_free_content(&msg.contents[0]);
        free(msg.contents);
    }

    bool continue_loop = true;
    while (continue_loop) {
        for (int iter = 0; iter < agent->max_iterations; iter++) {
            // [Checkpoint 1] Merge Steering messages
            consume_queue_to_context(agent, &agent->steering_queue);
            agent->abort_requested = false;

            tool_call_queue_t queue = {0};
            agent_callback_ctx_t ctx = {callback, agent->user_data, &queue, agent};
            agent->options.abort_signal = &agent->abort_requested;

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

            // Execute tools with Steering checkpoints
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
                    // [Checkpoint 2] Steering check during tool stream
                    pthread_mutex_lock(&agent->mutex);
                    if (agent->steering_queue.count > 0) {
                        pthread_mutex_unlock(&agent->mutex);
                        break; // Interrupt tool sequence
                    }
                    pthread_mutex_unlock(&agent->mutex);

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

        // [Checkpoint 3] Check Follow-up Queue
        pthread_mutex_lock(&agent->mutex);
        if (agent->follow_up_queue.count > 0) {
            pthread_mutex_unlock(&agent->mutex);
            consume_queue_to_context(agent, &agent->follow_up_queue);
            // Re-enter the max_iterations loop
        } else {
            pthread_mutex_unlock(&agent->mutex);
            continue_loop = false;
        }
    }

    return 0;
}
