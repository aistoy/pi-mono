#ifndef PI_AI_H
#define PI_AI_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PI_AI_CONTENT_TEXT,
    PI_AI_CONTENT_THINKING,
    PI_AI_CONTENT_TOOL_CALL,
    PI_AI_CONTENT_IMAGE
} pi_ai_content_type_t;

typedef struct {
    pi_ai_content_type_t type;
    union {
        char *text;
        struct {
            char *thinking;
            char *signature;
            bool redacted;
        } thinking;
        struct {
            char *id;
            char *name;
            char *arguments_json;
        } tool_call;
        struct {
            char *data_base64;
            char *mime_type;
        } image;
    } data;
} pi_ai_content_t;

typedef struct {
    char *role; // "user", "assistant", "system", "toolResult"
    pi_ai_content_t *contents;
    size_t content_count;
    // For toolResult
    char *tool_call_id;
    char *tool_name;
    bool is_error;
} pi_ai_message_t;

typedef struct {
    char *name;
    char *description;
    char *parameters_json; // JSON Schema string
} pi_ai_tool_t;

typedef struct {
    char *system_prompt;
    pi_ai_message_t *messages;
    size_t message_count;
    pi_ai_tool_t *tools;
    size_t tool_count;
} pi_ai_context_t;

typedef enum {
    PI_AI_EVENT_START,
    PI_AI_EVENT_TEXT_START,
    PI_AI_EVENT_TEXT_DELTA,
    PI_AI_EVENT_TEXT_END,
    PI_AI_EVENT_THINKING_START,
    PI_AI_EVENT_THINKING_DELTA,
    PI_AI_EVENT_THINKING_END,
    PI_AI_EVENT_TOOLCALL_START,
    PI_AI_EVENT_TOOLCALL_DELTA,
    PI_AI_EVENT_TOOLCALL_END,
    PI_AI_EVENT_DONE,
    PI_AI_EVENT_ERROR
} pi_ai_event_type_t;

typedef struct {
    pi_ai_event_type_t type;
    size_t content_index;
    const char *delta; // For delta events
    const char *full_content; // For end events
    const char *tool_call_id;   // Added for Agent Loop
    const char *tool_call_name; // Added for Agent Loop
    void *raw_event_data; // Reserved for additional metadata
} pi_ai_event_t;

typedef void (*pi_ai_callback_t)(pi_ai_event_t *event, void *user_data);

// Memory Management
void pi_ai_free_content(pi_ai_content_t *content);
void pi_ai_free_message(pi_ai_message_t *message);
void pi_ai_free_context(pi_ai_context_t *context);
void pi_ai_free_tool(pi_ai_tool_t *tool);

// Helper to create messages
pi_ai_message_t* pi_ai_create_message(const char *role);
void pi_ai_message_add_text(pi_ai_message_t *message, const char *text);

#ifdef __cplusplus
}
#endif

#endif // PI_AI_H
