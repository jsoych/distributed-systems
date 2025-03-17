package main

import (
	"fmt"
	"strings"
)

var JobStatus = map[string]int{
	"Incomplete": -2,
	"Not_Ready":  -1,
	"Ready":      0,
	"Running":    1,
	"Completed":  2,
}

// Job class
type Job struct {
	Id     int
	Status string
	Tasks  []string
}

// String: Formats and returns the job as a string.
func (job Job) String() string {
	ss := make([]string, len(job.Tasks)+3)
	ss[0] = fmt.Sprintf("Id: %v", job.Id)
	ss[1] = fmt.Sprintf("Status: %v", job.Status)
	ss[2] = "Tasks:"
	for i, task := range job.Tasks {
		ss[i+3] = task
	}
	return strings.Join(ss, " ")
}

func main() {
	tasks := make([]string, 3)
	for i := range tasks {
		tasks[i] = fmt.Sprintf("task%v.py", i)
	}
	job := Job{42, "Incomplete", tasks}
	fmt.Println(job)
}
