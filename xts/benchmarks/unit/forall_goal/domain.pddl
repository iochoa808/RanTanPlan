;; Test: forall quantifier inside the :goal condition.
;;
;; forall is tested in preconditions and effects elsewhere; this is the only
;; test where it appears inside (:goal ...).  The goal expands to a conjunction
;; over all grounded values of ?i, requiring every cell to be 1.

(define (domain forall-goal)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        idx   - (number 0 2)
        val   - (number 0 1)
        arr_t - (array 3 val)
    )

    (:functions
        (arr) - arr_t
    )

    (:action fill
        :parameters (?i - idx)
        :precondition (= (read (arr) ?i) 0)
        :effect (write (arr) (?i) 1)
    )
)
