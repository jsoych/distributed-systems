#include <stdio.h>
#include <string.h>

#include "json-helpers.h"

/* json_object_get_value: Gets the value from the object by its name. */
json_value* json_object_get_value(const json_value *obj, const char* name) {
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
    return 0;
}

/* json_object_push_integer: Creates and pushes a new integer to the obj. */
int json_object_push_integer(json_value* obj, const char* key, int integer) {
    json_value* val = json_integer_new(integer);
    if (val == NULL) return -1;
    val = json_object_push(obj, key, val);
    if (val == NULL) return -1;
    return 0;
}

/* json_value_compare: Compares the values and returns 0 if they are equalivent
    and 1 if they are not equalivent. */
int json_value_compare(const json_value* value1, const json_value* value2) {
    // base cases
    if (value1->type != value2->type) return 1;

    // json_none
    if (value1->type == json_none) return 0;

    // json_null
    if (value1->type == json_null) return 0;

    // json_integer
    if (value1->type == json_integer) {
        if (value1->u.integer == value2->u.integer) return 0;
        return 1;
    }

    // json_double
    if (value1->type == json_double) {
        if (value1->u.dbl == value2->u.dbl) return 0;
        return 1;
    }

    // json_boolean
    if (value1->type == json_boolean) {
        if (value1->u.boolean == value2->u.boolean) return 0;
        return 1;
    }

    // json_string
    if (value1->type == json_string) {
        if (strcmp(value1->u.string.ptr, value2->u.string.ptr) == 0) return 0;
        return 1;
    }

    // json_array
    if (value1->type == json_array) {
        if (value1->u.array.length != value2->u.array.length) return 1;
        for (unsigned int i = 0; i < value1->u.array.length; i++) {
            int result = json_value_compare(
                value1->u.array.values[i], value2->u.array.values[i]
            );
            if (result != 0) return result;
        }
        return 0;
    }

    // json_object
    if (value1->type == json_object) {
        if (value1->u.object.length != value2->u.object.length) {
            printf("object1.length: %d\n", value1->u.object.length);
            return 1;
        }
        for (unsigned int i = 0; i < value1->u.object.length; i++) {
            json_value* val = json_object_get_value(
                value2, value1->u.object.values[i].name
            );
            if (val == NULL) return 1;
            int result = json_value_compare(value1->u.object.values[i].value, val);
            if (result != 0) return result;
        }
        return 0;
    }

    return -1;
}
