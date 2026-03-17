#include "pi_ai.h"
#include <stdlib.h>
#include <string.h>

void pi_ai_free_content(pi_ai_content_t *content) {
    if (!content) return;
    switch (content->type) {
        case PI_AI_CONTENT_TEXT:
            free(content->data.text);
            break;
        case PI_AI_CONTENT_THINKING:
            free(content->data.thinking.thinking);
            free(content->data.thinking.signature);
            break;
        case PI_AI_CONTENT_TOOL_CALL:
            free(content->data.tool_call.id);
            free(content->data.tool_call.name);
            free(content->data.tool_call.arguments_json);
            break;
        case PI_AI_CONTENT_IMAGE:
            free(content->data.image.data_base64);
            free(content->data.image.mime_type);
            break;
    }
}

void pi_ai_free_message(pi_ai_message_t *message) {
    if (!message) return;
    free(message->role);
    free(message->tool_call_id);
    free(message->tool_name);
    for (size_t i = 0; i < message->content_count; i++) {
        pi_ai_free_content(&message->contents[i]);
    }
    free(message->contents);
}

void pi_ai_free_tool(pi_ai_tool_t *tool) {
    if (!tool) return;
    free(tool->name);
    free(tool->description);
    free(tool->parameters_json);
}

void pi_ai_free_context(pi_ai_context_t *context) {
    if (!context) return;
    free(context->system_prompt);
    for (size_t i = 0; i < context->message_count; i++) {
        pi_ai_free_message(&context->messages[i]);
    }
    free(context->messages);
    for (size_t i = 0; i < context->tool_count; i++) {
        pi_ai_free_tool(&context->tools[i]);
    }
    free(context->tools);
}

pi_ai_message_t* pi_ai_create_message(const char *role) {
    pi_ai_message_t *msg = calloc(1, sizeof(pi_ai_message_t));
    if (msg) {
        msg->role = strdup(role);
    }
    return msg;
}

void pi_ai_message_add_text(pi_ai_message_t *message, const char *text) {
    message->contents = realloc(message->contents, sizeof(pi_ai_content_t) * (message->content_count + 1));
    pi_ai_content_t *content = &message->contents[message->content_count];
    memset(content, 0, sizeof(pi_ai_content_t));
    content->type = PI_AI_CONTENT_TEXT;
    content->data.text = strdup(text);
    message->content_count++;
}
