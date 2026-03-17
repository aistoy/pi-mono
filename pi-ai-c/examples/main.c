#include "pi_ai.h"
#include "pi_ai_openai.h"
#include "pi_ai_validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void my_callback(pi_ai_event_t *event, void *user_data) {
    switch (event->type) {
        case PI_AI_EVENT_START:
            printf("\n[Assistant Started]\n");
            break;
        case PI_AI_EVENT_TEXT_DELTA:
            printf("%s", event->delta);
            fflush(stdout);
            break;
        case PI_AI_EVENT_THINKING_DELTA:
            printf("[Thinking: %s]", event->delta);
            fflush(stdout);
            break;
        case PI_AI_EVENT_TOOLCALL_START:
            printf("\n[Tool Call Started]\n");
            break;
        case PI_AI_EVENT_TOOLCALL_DELTA:
            printf("[Tool Args Delta: %s]", event->delta);
            break;
        case PI_AI_EVENT_TOOLCALL_END:
            printf("\n[Tool Call Ended] Full Args: %s\n", event->full_content);
            break;
        case PI_AI_EVENT_DONE:
            printf("\n[Done]\n");
            break;
        case PI_AI_EVENT_ERROR:
            printf("\n[Error] %s\n", event->delta);
            break;
        default:
            break;
    }
}

int main() {
    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        printf("Please set OPENAI_API_KEY environment variable.\n");
        return 1;
    }

    pi_ai_context_t context = {0};
    context.system_prompt = strdup("You are a helpful assistant.");

    // Add a user message
    context.messages = calloc(1, sizeof(pi_ai_message_t));
    context.messages[0].role = strdup("user");
    pi_ai_message_add_text(&context.messages[0], "What is the weather in Tokyo? Use a tool if available.");
    context.message_count = 1;

    // Add a tool
    context.tools = calloc(1, sizeof(pi_ai_tool_t));
    context.tools[0].name = strdup("get_weather");
    context.tools[0].description = strdup("Get current weather for a location");
    context.tools[0].parameters_json = strdup("{\"type\":\"object\",\"properties\":{\"location\":{\"type\":\"string\"}},\"required\":[\"location\"]}");
    context.tool_count = 1;

    pi_ai_openai_options_t options = {
        .api_base_url = "https://api.openai.com/v1/chat/completions",
        .api_key = api_key,
        .model_id = "gpt-4o-mini",
        .temperature = 0.7,
        .max_tokens = 1000
    };

    printf("Starting stream...\n");
    pi_ai_openai_stream(&options, &context, my_callback, NULL);

    // Memory Cleanup
    pi_ai_free_context(&context);

    return 0;
}
