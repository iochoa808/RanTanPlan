;; Test: whole 3D array equality in goal.
;;
;; Covers: (array.mk ...) literal in a goal for a 3D array; extends 2d_whole_goal.
;;
;; Domain: same 2×2×2 integer tensor as the base 3d test.

(define (domain test-3d-whole)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        depth - (number 0 1)
        row   - (number 0 1)
        col   - (number 0 1)
        val   - (number 0 5)
        cube  - (array 2 2 2 val)
    )

    (:functions
        (tensor) - cube
    )

    (:action set
        :parameters (?d - depth ?r - row ?c - col ?v - val)
        :precondition (and
            (> ?v 0)
            (= (read (tensor) ?d ?r ?c) 0)
        )
        :effect (write ((tensor) ?d ?r ?c) ?v)
    )
)
