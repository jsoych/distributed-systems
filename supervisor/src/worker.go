package main

import "fmt"

// Worker object
type Worker struct {
	Id   int
	Addr [4]byte
}

// String: Formats and returns the worker as a string.
func (worker *Worker) String() string {
	return fmt.Sprintf("Id: %v Addr: %v.%v.%v.%v", worker.Id, worker.Addr)
}
