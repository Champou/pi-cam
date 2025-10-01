#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON/cJSON.h"
#include "json_wrapper.h"

bool json_validate(const char *json_str) {
    if (!json_str) return false;
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }
    cJSON_Delete(root);
    return true;
}

char* json_search(const char *json_str, const char *key) {
    if (!json_str || !key) return NULL;
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return NULL;

    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item)) {
        cJSON_Delete(root);
        return NULL;
    }

    // Duplicate string so caller owns it
    char *result = strdup(item->valuestring);

    cJSON_Delete(root);
    return result;
}
