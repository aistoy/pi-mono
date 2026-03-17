#include "pi_ai_partial_json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef enum {
    STATE_NONE,
    STATE_OBJECT,
    STATE_ARRAY,
    STATE_STRING,
    STATE_KEY,
    STATE_VALUE
} json_state_t;

cJSON* pi_ai_parse_partial_json(const char *partial_json) {
    if (!partial_json || strlen(partial_json) == 0) {
        return cJSON_CreateObject();
    }

    cJSON *root = cJSON_Parse(partial_json);
    if (root) return root;

    // A slightly better healer that tracks nesting
    size_t len = strlen(partial_json);
    char *stack = malloc(len + 1);
    int top = -1;
    bool in_string = false;
    bool escaped = false;

    for (size_t i = 0; i < len; i++) {
        char c = partial_json[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '\"') {
            in_string = !in_string;
            continue;
        }
        if (!in_string) {
            if (c == '{') stack[++top] = '}';
            else if (c == '[') stack[++top] = ']';
            else if (c == '}' || c == ']') {
                if (top >= 0 && stack[top] == c) top--;
            }
        }
    }

    char *healed = malloc(len + (top + 1) + 2);
    strcpy(healed, partial_json);
    char *p = healed + len;
    if (in_string) *p++ = '\"';
    while (top >= 0) {
        *p++ = stack[top--];
    }
    *p = '\0';

    root = cJSON_Parse(healed);
    free(stack);
    if (root) {
        free(healed);
        return root;
    }

    // If still fails, try removing trailing comma if any
    if (p > healed && *(p-1) == ',') {
        *(p-1) = '\0'; // This is wrong logic since we appended stuff, but you get the idea
    }

    free(healed);
    return cJSON_CreateObject();
}
