#include <stdbool.h>
#include <unistd.h>
#include "crew.h"
#include "unittest.h"

#define success unittest_success
#define failure unittest_failure
#define printr(result, name, msg) unittest_print_result("test-crew", result, name, msg)

bool check_table(Crew *);


int main() {
    Crew *crew;
    json_value *arr, *cmd, *job;

    // create empty crew
    crew = create_crew();

    // check length
    if (crew->len == 0)
        printr(success, "empty crew length", NULL);
    else
        printr(failure, "empty crew length", "unexpected length");

    // check each list length
    if (check_table(crew) == true)
        printr(success, "empty crew table check", NULL);
    else
        printr(failure, "empty crew table check", "inconsistent list length");

    // add worker
    add_worker(crew, 3);

    // check length
    if (crew->len == 1)
        printr(success, "add worker length", NULL);
    else
        printr(failure, "add worker length", "unexpected length");

    // check worker status
    if (get_worker_status(crew, 3) == worker_not_assigned)
        printr(success, "worker job status", NULL);
    else
        printr(failure, "worker job status", "unexpected worker status");

    add_worker(crew, 42);

    if (crew->len == 2)
        printr(success, "add worker length", NULL);
    else
        printr(success, "add worker length", "unexpected length");

    // add duplicate worker
    if (add_worker(crew, 3) == -1)
        printr(success, "add duplicate worker", NULL);
    else
        printr(failure, "add duplicate worker", "added duplicate worker to the crew");

    // assign job
    arr = json_array_new(2);
    json_array_push(arr, json_string_new("task.py"));
    json_array_push(arr, json_string_new("long.py"));
    job = json_object_new(0);
    json_object_push(job, "id", json_integer_new(1));
    json_object_push(job, "tasks", arr);

    if (assign_job(crew, job) > 0)
        printr(success, "assign job", NULL);
    else
        printr(failure, "assign job", "failed to assig job");

    json_builder_free(job);

    // check worker debugging logs
    sleep(10);
    
    free_crew(crew);
    return 0;
}

// check_table: Checks the lengths of all the lists in the table.
bool check_table(Crew *crew) {
    int len = 0;
    for (int i = 0; i < _CREW_MAXLEN; i++)
        len += crew->workers[i].len;
    return (crew->len == len);
}
