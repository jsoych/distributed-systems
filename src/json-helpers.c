#include "json-helpers.h"

/* json_object_get_value: Gets the value from the object by its name. */
json_value* json_object_get_value(json_value *obj, const char* name) {
    json_value* val = NULL;
    for (unsigned int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, name) == 0) {
            val = obj->u.object.values[i].value;
            break;
        }
    }
    return val;
}

/* json_object_push_string: Creates and pushes a new string to the object
    and returns 0. Otherwise, returns -1. */
int json_object_push_string(json_value* obj, const char* key, const char* str) {
    json_value* val = json_string_new(str);
    if (val == NULL) return -1;
    val = json_object_push(obj, key, val);
    if (val == NULL) return -1;
}
