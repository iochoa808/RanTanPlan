;; Test: conditional effect whose guard is FALSE — the cell must stay unchanged.
;;
;; conditional_array tests the case where (when guard ...) fires (guard=true).
;; This test covers the opposite: the guard is false at runtime, so the
;; conditional write is skipped and the frame axiom must preserve that cell.
;;
;; One action has two effects on the same array:
;;   - unconditional write to slot 1 (always fires)
;;   - conditional write to slot 0 (fires only when (guard) is true)
;; With (guard)=false in the initial state: slot 0 must remain 0 after the action.

(define (domain cond-noop)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        val    - (number 0 9)
        board_t - (array 2 val)
    )

    (:predicates
        (guard)
    )

    (:functions
        (board) - board_t
    )

    (:action fire
        :parameters ()
        :precondition (= (read (board) 1) 0)
        :effect (and
            (write (board) (1) 3)                    ;; always: slot 1 ← 3
            (when (guard) (write (board) (0) 9))     ;; conditional: slot 0 ← 9 only if guard
        )
    )
)
