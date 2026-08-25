;; Test: whole-array ASSIGN of a 2D array, then a constant cell write on it.
;;
;; KEY PATTERNS:
;;   1. (assign (pad) (array.mk (1 2) (3 4))) — whole-array replacement with a
;;      NESTED array.mk value.  whole_array_replace tests this for 1D only;
;;      nested ARRAY_CONSTANT as an effect value is otherwise untested.
;;   2. A follow-up (write ((pad) 1 1) 9) on the replaced array — the
;;      empty-indices replacement record and the point-write record coexist
;;      on the same parent SV in the frame-axiom ITE chain.
;;
;; touch's precondition reads a value only present after load, forcing the
;; replace → write order across timesteps.

(define (domain whole-2d-replace)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val   - (number 0 9)
        pad_t - (array 2 2 val)
    )

    (:functions
        (pad) - pad_t
    )

    ;; Replace the whole 2x2 pad with a fixed pattern.
    (:action load
        :parameters ()
        :precondition (= (read (pad) (0) (0)) 0)
        :effect (assign (pad) (array.mk (1 2) (3 4)))
    )

    ;; Overwrite one cell of the loaded pattern.
    (:action touch
        :parameters ()
        :precondition (= (read (pad) (0) (0)) 1)
        :effect (write ((pad) 1 1) 9)
    )
)
