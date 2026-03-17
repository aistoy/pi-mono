#include "pi_ai_validation.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

void pi_ai_free_validation_result(pi_ai_validation_result_t result) {
    free(result.error_message);
}

static pi_ai_validation_result_t create_error(const char *fmt, ...) {
    pi_ai_validation_result_t res;
    res.valid = false;
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    res.error_message = strdup(buffer);
    return res;
}

pi_ai_validation_result_t pi_ai_validate_tool_call(const pi_ai_tool_t *tool, const char *arguments_json) {
    pi_ai_validation_result_t success = {true, NULL};

    if (!tool || !tool->parameters_json) return success;

    cJSON *schema = cJSON_Parse(tool->parameters_json);
    if (!schema) return create_error("Invalid tool parameter schema JSON");

    cJSON *args = cJSON_Parse(arguments_json);
    if (!args) {
        cJSON_Delete(schema);
        return create_error("Invalid arguments JSON");
    }

    // Check required fields
    cJSON *required = cJSON_GetObjectItemCaseSensitive(schema, "required");
    if (cJSON_IsArray(required)) {
        int size = cJSON_GetArraySize(required);
        for (int i = 0; i < size; i++) {
            cJSON *req_field = cJSON_GetArrayItem(required, i);
            if (cJSON_IsString(req_field)) {
                if (!cJSON_HasObjectItem(args, req_field->valuestring)) {
                    pi_ai_validation_result_t err = create_error("Missing required parameter: %s", req_field->valuestring);
                    cJSON_Delete(schema);
                    cJSON_Delete(args);
                    return err;
                }
            }
        }
    }

    // Basic Type Checking
    cJSON *properties = cJSON_GetObjectItemCaseSensitive(schema, "properties");
    if (cJSON_IsObject(properties)) {
        cJSON *prop = NULL;
        cJSON_ArrayForEach(prop, properties) {
            cJSON *arg_val = cJSON_GetObjectItemCaseSensitive(args, prop->string);
            if (arg_val) {
                cJSON *type_node = cJSON_GetObjectItemCaseSensitive(prop, "type");
                if (cJSON_IsString(type_node)) {
                    const char *type = type_node->valuestring;
                    bool type_match = false;
                    if (strcmp(type, "string") == 0) type_match = cJSON_IsString(arg_val);
                    else if (strcmp(type, "number") == 0) type_match = cJSON_IsNumber(arg_val);
                    else if (strcmp(type, "integer") == 0) type_match = cJSON_IsNumber(arg_val);
                    else if (strcmp(type, "boolean") == 0) type_match = cJSON_IsBool(arg_val);
                    else if (strcmp(type, "object") == 0) type_match = cJSON_IsObject(arg_val);
                    else if (strcmp(type, "array") == 0) type_match = cJSON_IsArray(arg_val);
                    else type_match = true;

                    if (!type_match) {
                        pi_ai_validation_result_t err = create_error("Parameter '%s' expects type '%s'", prop->string, type);
                        cJSON_Delete(schema);
                        cJSON_Delete(args);
                        return err;
                    }
                }
            }
        }
    }

    cJSON_Delete(schema);
    cJSON_Delete(args);
    return success;
}
