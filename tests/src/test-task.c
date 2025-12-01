#include "task.h"

int main(int argc, char *argv) {
    Task *t;

    // simple task
    t = create_task("task.py");
    if (run_task(t)  == -1) {
        return -1;
    }
    free_task(t);

    // task with bug
    t = create_task("bug.py");
    if (run_task(t) != -1) {
        return -1;
    }
    free_task(t);
    
    return 0;
}
