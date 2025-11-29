#ifndef _JSON_HELPERS_H
#define _JSON_HELPERS_H

#include "common.h"
#include "json.h"
#include "json-builder.h"

json_value* json_get_object_value(json_value* object, const char* key);
const char* json_get_string_value(json_value* object, const char* key);
int json_get_int_value(json_value* object, const char* key);
double json_get_double_value(json_value* object, const char* key);
bool json_get_bool_value(json_value* object, const char* key);

#endif