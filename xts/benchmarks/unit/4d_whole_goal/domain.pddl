;; Test: whole 4D array equality in goal.
;;
;; Covers: (array.mk ...) literal in a goal for a 4D array; extends 3d_whole_goal.
;;
;; Domain: same 2×2×2×2 integer hypercube as the base 4d test.

(define (domain test-4d-whole)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        d0  - (number 0 1)
        d1  - (number 0 1)
        d2  - (number 0 1)
        d3  - (number 0 1)
        val - (number 0 3)
        hc  - (array 2 2 2 2 val)
    )

    (:functions
        (hypercube) - hc
    )

    (:action set
        :parameters (?a - d0 ?b - d1 ?c - d2 ?e - d3 ?v - val)
        :precondition (and
            (> ?v 0)
            (= (read (hypercube) ?a ?b ?c ?e) 0)
        )
        :effect (write ((hypercube) ?a ?b ?c ?e) ?v)
    )
)
