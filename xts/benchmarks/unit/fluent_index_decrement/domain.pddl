;; Test: READ-MODIFY-WRITE with DECREMENT — (write arr (?i) (- (read arr ?i) 1)).
;;
;; KEY PATTERN: (write (cells) (?i) (- (read (cells) ?i) 1))
;;   fluent_index tests INCREMENT (+1); this tests the DECREMENT (-1) direction.
;;   Verifies that the read-before-write chain works for subtraction.
;;
;; Domain: a 3-cell score array; decrement a cell if it is above 0.
;; Initial: cells=[3,0,0]; goal: cells[0]=0.
;; Plan: dec(0), dec(0), dec(0)  — 3 steps.

(define (domain score-down)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 2)
        score - (number 0 5)
        board - (array 3 score)
    )

    (:functions
        (cells) - board
    )

    ;; Decrement the score at slot ?i if it is strictly above 0.
    (:action dec
        :parameters (?i - slot)
        :precondition (> (read (cells) ?i) 0)
        :effect (write (cells) (?i) (- (read (cells) ?i) 1))
    )
)
