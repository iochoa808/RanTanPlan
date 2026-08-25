;; Test: whole-array equality (= (a) (array.mk ...)) used in a PRECONDITION.
;;
;; KEY PATTERN: existing tests use whole-array equality only in GOALS
;;   (whole_array_replace, 2d_whole_goal).  Here the same equality gates an
;;   action precondition, so the ARRAY_CONSTANT must be built and equated
;;   against the live array variable at the action's timestep, not only at
;;   the goal horizon.

(define (domain whole-precond)
    (:requirements :typing :arrays :bounded-integers :fluents)

    (:types
        val   - (number 0 9)
        arr_t - (array 3 val)
    )

    (:predicates (done))

    (:functions
        (a) - arr_t
    )

    ;; Fires only when the whole array already equals the target pattern.
    (:action finish
        :parameters ()
        :precondition (= (a) (array.mk (1 2 3)))
        :effect (done)
    )

    ;; A writer so the precondition is reached by planning, not just trivially.
    (:action set2
        :parameters ()
        :precondition (= (read (a) (2)) 0)
        :effect (write (a) (2) 3)
    )
)
