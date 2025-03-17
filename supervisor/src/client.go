package main

// workers list
type node struct {
	worker *Worker
	job    *Job
	next   *node
	prev   *node
}

type workers struct {
	head *node
}

// createWorkers: Creates a new empty workers list.
func createWorkers() *workers {
	n := node{}
	n.next = &n
	n.prev = &n
	return &workers{&n}
}

// append: Appends a new worker to the end of the workers list.
func (l *workers) add(id int, addr [4]byte) {
	w := Worker{id, addr}
	n := node{&w, nil, nil, nil}
	n.next = l.head
	l.head.prev.next = &n
	n.prev = l.head.prev
	l.head.prev = &n
}
