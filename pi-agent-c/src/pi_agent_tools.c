#include "pi_agent.h"
#include <stdlib.h>
#include <string.h>

pi_agent_t* pi_agent_create(const pi_ai_openai_options_t *options) {
    pi_agent_t *agent = calloc(1, sizeof(pi_agent_t));
    if (agent) {
        agent->options = *options;
        agent->max_iterations = 10;
        agent->tool_execution_mode = PI_AGENT_TOOL_EXECUTION_PARALLEL;
        pthread_mutex_init(&agent->mutex, NULL);
    }
    return agent;
}

static void free_queue(pi_agent_msg_queue_t *queue) {
    pi_agent_msg_node_t *curr = queue->head;
    while (curr) {
        pi_agent_msg_node_t *next = curr->next;
        pi_ai_free_message(curr->message);
        free(curr->message);
        free(curr);
        curr = next;
    }
    queue->head = queue->tail = NULL;
    queue->count = 0;
}

void pi_agent_free(pi_agent_t *agent) {
    if (!agent) return;
    pi_ai_free_context(&agent->context);
    for (size_t i = 0; i < agent->agent_tool_count; i++) {
        pi_ai_free_tool(&agent->agent_tools[i].info);
    }
    free(agent->agent_tools);

    free_queue(&agent->steering_queue);
    free_queue(&agent->follow_up_queue);
    pthread_mutex_destroy(&agent->mutex);

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

static void enqueue(pi_agent_msg_queue_t *queue, const char *text) {
    pi_ai_message_t *msg = pi_ai_create_message("user");
    pi_ai_message_add_text(msg, text);

    pi_agent_msg_node_t *node = calloc(1, sizeof(pi_agent_msg_node_t));
    node->message = msg;

    if (queue->tail) {
        queue->tail->next = node;
        queue->tail = node;
    } else {
        queue->head = queue->tail = node;
    }
    queue->count++;
}

void pi_agent_steer(pi_agent_t *agent, const char *text) {
    pthread_mutex_lock(&agent->mutex);
    enqueue(&agent->steering_queue, text);
    agent->abort_requested = true; // Signal for stream interruption
    pthread_mutex_unlock(&agent->mutex);
}

void pi_agent_follow_up(pi_agent_t *agent, const char *text) {
    pthread_mutex_lock(&agent->mutex);
    enqueue(&agent->follow_up_queue, text);
    pthread_mutex_unlock(&agent->mutex);
}
