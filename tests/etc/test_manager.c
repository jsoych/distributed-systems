#include <stdio.h>
#include <string.h>
#include "manager.h"
#include "unittest.h"

#define success UNITTEST_SUCCESS
#define failure UNITTEST_FAILURE
#define printr(result, name, msg) unittest_print_result("test-manager", result, name, msg)


int main() {
    Manager *man = create_manager(3);
    Project *proj;

    // check id
    if (man->id == 3)
        printr(success, "manager id", NULL);
    else
        printr(failure, "manager id", "unexpected value");

    // check manager status
    if (man->status == MANAGER_NOT_ASSIGNED)
        printr(success, "manager status", NULL);
    else
        printr(failure, "manager status", "unexpected value");

    // get status
    if (get_status(man) == MANAGER_NOT_ASSIGNED)
        printr(success, "get_status", NULL);
    else
        printr(failure, "get_status", "unexpected value");

    // get project status
    if (get_project_status(man) == -1)
        printr(success, "get_project_status", NULL);
    else
        printr(failure, "get_project_status", "unexpected value");

    // check project

    // run project

    // stop project

    // start project

    free_manager(man);

    return 0;
}
