#include <stdbool.h>

// Validate JSON string (returns true if valid)
bool json_validate(const char *json_str);

// Search value by key (returns malloc'd string, caller must free)
// Returns NULL if not found
char* json_search(const char *json_str, const char *key);

