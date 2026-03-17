#include "pi_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Hook: Log tool calls
pi_agent_before_tool_call_result_t my_before_hook(pi_agent_hook_context_t *ctx) {
    printf("\n>>> [Hook] Preparing to call %s with args: %s\n", ctx->tool_name, cJSON_PrintUnformatted(ctx->arguments));

    pi_agent_before_tool_call_result_t res = {false, NULL};
    // Example: Block a specific tool call
    if (strcmp(ctx->tool_name, "forbidden_tool") == 0) {
        res.block = true;
        res.reason = strdup("This tool is not allowed by policy.");
    }
    return res;
}

// Hook: Modify tool results
pi_agent_after_tool_call_result_t my_after_hook(pi_agent_hook_context_t *ctx, pi_agent_tool_result_t result) {
    printf(">>> [Hook] %s finished. Original error state: %s\n", ctx->tool_name, result.is_error ? "true" : "false");

    pi_agent_after_tool_call_result_t res = {NULL, false, false};
    // Example: If result contains a certain value, mark it as error
    return res;
}

// Tool: Add
pi_agent_tool_result_t tool_add(const char *id, const cJSON *args, void *user_data) {
    cJSON *a = cJSON_GetObjectItem(args, "a");
    cJSON *b = cJSON_GetObjectItem(args, "b");
    double res = (a ? a->valuedouble : 0) + (b ? b->valuedouble : 0);

    printf("[Tool Add] %f + %f = %f\n", (a?a->valuedouble:0), (b?b->valuedouble:0), res);

    pi_agent_tool_result_t result = {0};
    result.content = cJSON_CreateObject();
    cJSON_AddNumberToObject(result.content, "result", res);
    return result;
}

void agent_event_callback(pi_ai_event_t *event, void *user_data) {
    if (event->type == PI_AI_EVENT_TEXT_DELTA) {
        printf("%s", event->delta);
        fflush(stdout);
    }
}

int main() {
    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        printf("Please set OPENAI_API_KEY.\n");
        return 1;
    }

    pi_ai_openai_options_t options = {
        .api_base_url = "https://api.openai.com/v1/chat/completions",
        .api_key = api_key,
        .model_id = "gpt-4o-mini",
        .temperature = 0,
        .max_tokens = 1000
    };

    pi_agent_t *agent = pi_agent_create(&options);

    // Set hooks
    agent->before_tool_call = my_before_hook;
    agent->after_tool_call = my_after_hook;

    // Set retry
    agent->max_retries_on_error = 2;
    agent->retry_delay_ms = 1000;

    const char *math_schema = "{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"number\"},\"b\":{\"type\":\"number\"}},\"required\":[\"a\",\"b\"]}";
    pi_agent_register_tool(agent, "add", "Add two numbers", math_schema, tool_add);

    printf("Agent ready with Hooks and Retry. Prompt: '123 + 456'\n");
    pi_agent_run(agent, "What is 123 + 456?", agent_event_callback);

    pi_agent_free(agent);
    return 0;
}
