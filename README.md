# Pyoneer
Pyoneer is a distributed system designed to extend a local python environment to many machines. Since python programs range in complexity, Pyoneer defines a collection of resources that structure the program proportional to its needs. For example, if we needs to run several related tasks, we can create a resource that bundles together the related tasks and execute it on a remote machine. The goal of the Pyoneer is to be very light in configuration and fun add build to :).


## Definitions
### kind
Kind specifies the type of resource in the distributed system. For example, kind can equal task, job or project, where each value corresponds to a unique resource.

### blueprint
A blueprint is a resource within the distributed system. Each blueprint has a unique kind and can be composed of other blueprints.

### status
Status refers to the state of the agent or the resource. For example, the status of an agent can be equal to assigned or not working, and the status of a resource can equal not ready or ruuning.

### pyoneer
A pyoneer is an agent in the distributed system. Each pyoneer has a unique role and a unique blueprint that it works on. To avoid confusion with the name of the distributed system, we always use an uppercase when referring to the distributed system (Pyoneer) and a lowercase when referring to an agent (pyoneer). 


## Core Design
- the state of the system is the product of each pyoneer and its blueprint
- there is a 1-to-1 relationship between pyoneers and blueprints in the system
- blueprints are statusless in and of itself, and its status is determined only in the context of a pyoneer
- projects are made up of jobs with dependencies, jobs are made up of tasks, and tasks are python scripts
- objects are thread safe, composable and resuable, and data structures are not thread safe and not resuable
- pyoneers inherits from the pyoneer base class and must implement the methods: get_status, get_blueprint_status, run, assign, unassign, stop, start
- managers communicate with workers, workers communicate with the machine.
- worker are "lazy" and will idle after completing a job, and workers don't communicate with other workers
