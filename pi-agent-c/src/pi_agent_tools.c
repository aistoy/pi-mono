#include "pi_agent.h"
#include <stdlib.h>
#include <string.h>

pi_agent_t* pi_agent_create(const pi_ai_openai_options_t *options) {
    pi_agent_t *agent = calloc(1, sizeof(pi_agent_t));
    if (agent) {
        agent->options = *options;
        agent->max_iterations = 10;
        agent->tool_execution_mode = PI_AGENT_TOOL_EXECUTION_PARALLEL;
    }
    return agent;
}

void pi_agent_free(pi_agent_t *agent) {
    if (!agent) return;
    pi_ai_free_context(&agent->context);
    for (size_t i = 0; i < agent->agent_tool_count; i++) {
        pi_ai_free_tool(&agent->agent_tools[i].info);
    }
    free(agent->agent_tools);
    free(agent);
}

void pi_agent_register_tool(pi_agent_t *agent, const char *name, const char *description, const char *parameters_json, pi_agent_tool_proc_t proc) {
    agent->agent_tools = realloc(agent->agent_tools, sizeof(pi_agent_tool_t) * (agent->agent_tool_count + 1));
    pi_agent_tool_t *atool = &agent->agent_tools[agent->agent_tool_count];
    atool->info.name = strdup(name);
    atool->info.description = strdup(description);
    atool->info.parameters_json = strdup(parameters_json);
    atool->proc = proc;

    // Also sync to pi_ai tools for LLM awareness
    agent->context.tools = realloc(agent->context.tools, sizeof(pi_ai_tool_t) * (agent->context.tool_count + 1));
    agent->context.tools[agent->context.tool_count].name = strdup(name);
    agent->context.tools[agent->context.tool_count].description = strdup(description);
    agent->context.tools[agent->context.tool_count].parameters_json = strdup(parameters_json);

    agent->agent_tool_count++;
    agent->context.tool_count++;
}
