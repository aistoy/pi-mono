#include "pi_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

void* simulator_thread(void *arg) {
    pi_agent_t *agent = (pi_agent_t *)arg;

    // Simulate user waiting 3 seconds then steering the agent
    printf("[Simulator] User is watching...\n");
    sleep(3);
    printf("\n[Simulator] User INJECTS new instruction: 'Wait! Stop what you are doing and just tell me a joke instead.'\n");
    pi_agent_steer(agent, "Wait! Stop what you are doing and just tell me a joke instead.");

    // Add a follow-up task
    printf("[Simulator] User queues a Follow-up task: 'Now tell me the time.'\n");
    pi_agent_follow_up(agent, "Now tell me the current time.");

    return NULL;
}

void agent_callback(pi_ai_event_t *event, void *user_data) {
    if (event->type == PI_AI_EVENT_TEXT_DELTA) {
        printf("%s", event->delta);
        fflush(stdout);
    }
}

int main() {
    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        printf("Set OPENAI_API_KEY.\n");
        return 1;
    }

    pi_ai_openai_options_t options = {
        .api_base_url = "https://api.openai.com/v1/chat/completions",
        .api_key = api_key,
        .model_id = "gpt-4o-mini",
        .temperature = 0.7,
        .max_tokens = 500
    };

    pi_agent_t *agent = pi_agent_create(&options);

    pthread_t sim;
    pthread_create(&sim, NULL, simulator_thread, agent);

    printf("Agent started on a long task: 'Write a 500 word essay about the history of C language.'\n");
    pi_agent_run(agent, "Write a 500 word essay about the history of C language.", agent_callback);

    pthread_join(sim, NULL);
    pi_agent_free(agent);
    printf("\nDone.\n");
    return 0;
}
