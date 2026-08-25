;; Test domain: 1D array of sets-of-bounded-integers.
;;
;; Covers: (array N task_set) where task_set = (set task) and task is a bounded int.
;;
;; Each bay holds a set of task IDs.  The copy_tasks action reads a whole
;; set from one bay and writes it to another (overwrite, not merge).
;;
;; Element type: set{bounded_int}   Container: 1D array

(define (domain task-router)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        bay      - (number 0 2)
        task     - (number 0 4)
        task_set - (set task)
        bay_map  - (array 3 task_set)
    )

    (:functions
        (assignments) - bay_map
    )

    (:action copy_tasks
        :parameters (?src - bay ?dst - bay)
        :precondition (not (= ?src ?dst))
        :effect (write (assignments) (?dst) (read (assignments) ?src))
    )
)
