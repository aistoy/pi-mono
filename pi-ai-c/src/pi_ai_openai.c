#include "pi_ai_openai.h"
#include "pi_ai_partial_json.h"
#include <curl/curl.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    pi_ai_callback_t callback;
    void *user_data;
    char *buffer;
    size_t buffer_len;

    // State tracking for partial content
    int current_content_index;
    char *accumulated_text;
    char *accumulated_thinking;
    char *accumulated_tool_args;
    char *current_tool_id;
    char *current_tool_name;

    bool started;
} openai_stream_ctx_t;

static void trigger_event(openai_stream_ctx_t *ctx, pi_ai_event_type_t type, const char *delta, const char *full) {
    pi_ai_event_t ev;
    ev.type = type;
    ev.content_index = ctx->current_content_index;
    ev.delta = delta;
    ev.full_content = full;
    ev.tool_call_id = ctx->current_tool_id;
    ev.tool_call_name = ctx->current_tool_name;
    ev.raw_event_data = NULL;
    ctx->callback(&ev, ctx->user_data);
}

static void process_sse_data(const char *data, openai_stream_ctx_t *ctx) {
    if (strcmp(data, "[DONE]") == 0) {
        trigger_event(ctx, PI_AI_EVENT_DONE, NULL, NULL);
        return;
    }

    cJSON *json = cJSON_Parse(data);
    if (!json) return;

    if (!ctx->started) {
        trigger_event(ctx, PI_AI_EVENT_START, NULL, NULL);
        ctx->started = true;
    }

    cJSON *choices = cJSON_GetObjectItem(json, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *choice = cJSON_GetArrayItem(choices, 0);
        cJSON *delta = cJSON_GetObjectItem(choice, "delta");
        if (delta) {
            // Text Content
            cJSON *content = cJSON_GetObjectItem(delta, "content");
            if (cJSON_IsString(content) && strlen(content->valuestring) > 0) {
                if (!ctx->accumulated_text) {
                    trigger_event(ctx, PI_AI_EVENT_TEXT_START, NULL, NULL);
                    ctx->accumulated_text = strdup("");
                }
                size_t old_len = strlen(ctx->accumulated_text);
                size_t delta_len = strlen(content->valuestring);
                ctx->accumulated_text = realloc(ctx->accumulated_text, old_len + delta_len + 1);
                strcat(ctx->accumulated_text, content->valuestring);
                trigger_event(ctx, PI_AI_EVENT_TEXT_DELTA, content->valuestring, NULL);
            }

            // Tool Calls
            cJSON *tool_calls = cJSON_GetObjectItem(delta, "tool_calls");
            if (cJSON_IsArray(tool_calls)) {
                cJSON *tool_call = cJSON_GetArrayItem(tool_calls, 0);
                cJSON *id = cJSON_GetObjectItem(tool_call, "id");
                cJSON *function = cJSON_GetObjectItem(tool_call, "function");

                if (id && cJSON_IsString(id)) {
                    // New tool call started
                    if (ctx->accumulated_tool_args) {
                        // End previous if any (simplified: assuming sequential for now)
                        trigger_event(ctx, PI_AI_EVENT_TOOLCALL_END, NULL, ctx->accumulated_tool_args);
                        free(ctx->accumulated_tool_args);
                        ctx->accumulated_tool_args = NULL;
                    }
                    ctx->current_tool_id = strdup(id->valuestring);
                    ctx->accumulated_tool_args = strdup("");

                    cJSON *name = cJSON_GetObjectItem(function, "name");
                    if (name && cJSON_IsString(name)) {
                        ctx->current_tool_name = strdup(name->valuestring);
                    }
                    trigger_event(ctx, PI_AI_EVENT_TOOLCALL_START, NULL, NULL);
                }

                if (function) {
                    cJSON *args = cJSON_GetObjectItem(function, "arguments");
                    if (cJSON_IsString(args) && strlen(args->valuestring) > 0) {
                        size_t old_len = strlen(ctx->accumulated_tool_args);
                        size_t delta_len = strlen(args->valuestring);
                        ctx->accumulated_tool_args = realloc(ctx->accumulated_tool_args, old_len + delta_len + 1);
                        strcat(ctx->accumulated_tool_args, args->valuestring);

                        // Partial JSON parsing for preview
                        cJSON *partial = pi_ai_parse_partial_json(ctx->accumulated_tool_args);
                        // We could pass this to callback via raw_event_data if needed
                        cJSON_Delete(partial);

                        trigger_event(ctx, PI_AI_EVENT_TOOLCALL_DELTA, args->valuestring, NULL);
                    }
                }
            }

            // Reasoning / Thinking (OpenAI reasoning_content or other fields)
            cJSON *reasoning = cJSON_GetObjectItem(delta, "reasoning_content");
            if (!reasoning) reasoning = cJSON_GetObjectItem(delta, "reasoning");

            if (cJSON_IsString(reasoning) && strlen(reasoning->valuestring) > 0) {
                if (!ctx->accumulated_thinking) {
                    trigger_event(ctx, PI_AI_EVENT_THINKING_START, NULL, NULL);
                    ctx->accumulated_thinking = strdup("");
                }
                size_t old_len = strlen(ctx->accumulated_thinking);
                size_t delta_len = strlen(reasoning->valuestring);
                ctx->accumulated_thinking = realloc(ctx->accumulated_thinking, old_len + delta_len + 1);
                strcat(ctx->accumulated_thinking, reasoning->valuestring);
                trigger_event(ctx, PI_AI_EVENT_THINKING_DELTA, reasoning->valuestring, NULL);
            }

            cJSON *finish_reason = cJSON_GetObjectItem(choice, "finish_reason");
            if (cJSON_IsString(finish_reason)) {
                // Finalize any open blocks
                if (ctx->accumulated_text) {
                    trigger_event(ctx, PI_AI_EVENT_TEXT_END, NULL, ctx->accumulated_text);
                }
                if (ctx->accumulated_thinking) {
                    trigger_event(ctx, PI_AI_EVENT_THINKING_END, NULL, ctx->accumulated_thinking);
                }
                if (ctx->accumulated_tool_args) {
                    trigger_event(ctx, PI_AI_EVENT_TOOLCALL_END, NULL, ctx->accumulated_tool_args);
                }
            }
        }
    }

    cJSON_Delete(json);
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total_size = size * nmemb;
    openai_stream_ctx_t *ctx = (openai_stream_ctx_t *)userdata;

    ctx->buffer = realloc(ctx->buffer, ctx->buffer_len + total_size + 1);
    memcpy(ctx->buffer + ctx->buffer_len, ptr, total_size);
    ctx->buffer_len += total_size;
    ctx->buffer[ctx->buffer_len] = '\0';

    char *line_start = ctx->buffer;
    char *next_line;
    while ((next_line = strstr(line_start, "\n"))) {
        *next_line = '\0';
        if (strncmp(line_start, "data: ", 6) == 0) {
            process_sse_data(line_start + 6, ctx);
        }
        line_start = next_line + 1;
    }

    size_t processed_len = line_start - ctx->buffer;
    size_t remaining_len = ctx->buffer_len - processed_len;
    if (remaining_len > 0) {
        memmove(ctx->buffer, line_start, remaining_len + 1);
        ctx->buffer_len = remaining_len;
    } else {
        ctx->buffer_len = 0;
    }

    return total_size;
}

int pi_ai_openai_stream(
    const pi_ai_openai_options_t *options,
    const pi_ai_context_t *context,
    pi_ai_callback_t callback,
    void *user_data
) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", options->api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", options->model_id);
    cJSON_AddBoolToObject(root, "stream", true);
    if (options->temperature >= 0) cJSON_AddNumberToObject(root, "temperature", options->temperature);

    // Use max_completion_tokens for newer models or max_tokens for older
    cJSON_AddNumberToObject(root, "max_tokens", options->max_tokens > 0 ? options->max_tokens : 4096);

    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    if (context->system_prompt) {
        cJSON *sys_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(sys_msg, "role", "system");
        cJSON_AddStringToObject(sys_msg, "content", context->system_prompt);
        cJSON_AddItemToArray(messages, sys_msg);
    }
    for (size_t i = 0; i < context->message_count; i++) {
        cJSON *m = cJSON_CreateObject();
        cJSON_AddStringToObject(m, "role", context->messages[i].role);

        // Handle toolResult role specifically for OpenAI
        if (strcmp(context->messages[i].role, "toolResult") == 0) {
             cJSON_ReplaceItemInObject(m, "role", cJSON_CreateString("tool"));
             cJSON_AddStringToObject(m, "tool_call_id", context->messages[i].tool_call_id);
        }

        if (context->messages[i].content_count > 0 && context->messages[i].contents[0].type == PI_AI_CONTENT_TEXT) {
            cJSON_AddStringToObject(m, "content", context->messages[i].contents[0].data.text);
        } else {
            cJSON_AddStringToObject(m, "content", "");
        }
        cJSON_AddItemToArray(messages, m);
    }

    if (context->tool_count > 0) {
        cJSON *tools = cJSON_AddArrayToObject(root, "tools");
        for (size_t i = 0; i < context->tool_count; i++) {
            cJSON *t = cJSON_CreateObject();
            cJSON_AddStringToObject(t, "type", "function");
            cJSON *f = cJSON_AddObjectToObject(t, "function");
            cJSON_AddStringToObject(f, "name", context->tools[i].name);
            cJSON_AddStringToObject(f, "description", context->tools[i].description);
            cJSON *params = cJSON_Parse(context->tools[i].parameters_json);
            cJSON_AddItemToObject(f, "parameters", params);
            cJSON_AddItemToArray(tools, t);
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    openai_stream_ctx_t stream_ctx = {0};
    stream_ctx.callback = callback;
    stream_ctx.user_data = user_data;

    curl_easy_setopt(curl, CURLOPT_URL, options->api_base_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream_ctx);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        pi_ai_event_t ev = {PI_AI_EVENT_ERROR, 0, curl_easy_strerror(res), NULL, NULL};
        callback(&ev, user_data);
    }

    free(json_str);
    free(stream_ctx.buffer);
    free(stream_ctx.accumulated_text);
    free(stream_ctx.accumulated_thinking);
    free(stream_ctx.accumulated_tool_args);
    free(stream_ctx.current_tool_id);
    free(stream_ctx.current_tool_name);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}
