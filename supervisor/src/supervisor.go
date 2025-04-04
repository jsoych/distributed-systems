/*
The purpose of the supervisor is to assign jobs to workers,
track job progress, and respond to requests from the manager.
*/
package supervisor

type supervisor struct {
	id     int
	status int
	crew   []worker
	jobs   []job
}

// updateStatus: Updates the worker status.
