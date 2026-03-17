#include "pi_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tool: Add
pi_agent_tool_result_t tool_add(const char *id, const cJSON *args, void *user_data) {
    cJSON *a = cJSON_GetObjectItem(args, "a");
    cJSON *b = cJSON_GetObjectItem(args, "b");
    double res = (a ? a->valuedouble : 0) + (b ? b->valuedouble : 0);

    pi_agent_tool_result_t result = {0};
    result.content = cJSON_CreateObject();
    cJSON_AddNumberToObject(result.content, "result", res);
    return result;
}

// Tool: Multiply
pi_agent_tool_result_t tool_multiply(const char *id, const cJSON *args, void *user_data) {
    cJSON *a = cJSON_GetObjectItem(args, "a");
    cJSON *b = cJSON_GetObjectItem(args, "b");
    double res = (a ? a->valuedouble : 0) * (b ? b->valuedouble : 0);

    pi_agent_tool_result_t result = {0};
    result.content = cJSON_CreateObject();
    cJSON_AddNumberToObject(result.content, "result", res);
    return result;
}

void agent_event_callback(pi_ai_event_t *event, void *user_data) {
    if (event->type == PI_AI_EVENT_TEXT_DELTA) {
        printf("%s", event->delta);
        fflush(stdout);
    } else if (event->type == PI_AI_EVENT_TOOLCALL_START) {
        printf("\n[Agent calling tool: %s...]\n", event->tool_call_name);
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

    // Register tools
    const char *math_schema = "{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"number\"},\"b\":{\"type\":\"number\"}},\"required\":[\"a\",\"b\"]}";
    pi_agent_register_tool(agent, "add", "Add two numbers", math_schema, tool_add);
    pi_agent_register_tool(agent, "multiply", "Multiply two numbers", math_schema, tool_multiply);

    printf("Agent ready. Prompt: 'What is (123 + 456) * 78?'\n");
    pi_agent_run(agent, "What is (123 + 456) * 78?", agent_event_callback);

    pi_agent_free(agent);
    printf("\nDone.\n");

    return 0;
}
