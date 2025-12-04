#ifndef _JSON_HELPERS_H
#define _JSON_HELPERS_H

#include "json.h"
#include "json-builder.h"

json_value* json_object_get_value(const json_value* object, const char* key);

int json_object_push_string(json_value* object, const char* key, const char* string);
int json_value_compare(const json_value* value1, const json_value* value2);

#endif
