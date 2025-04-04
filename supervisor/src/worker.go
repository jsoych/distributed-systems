package supervisor

// Worker Status lookup table

// Worker object
type worker struct {
	id     int
	status int
}

// String: Returns a string representation of the worker.

// getWorkerStatus: Gets the worker's status.
func (w *worker) getWorkerStatus() int {
	// Connect to the worker socket

	// Create command

	// Send command

	// Decode response

	// Return status
	return 0
}
