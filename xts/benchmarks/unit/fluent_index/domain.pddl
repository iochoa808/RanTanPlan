;; Test: WRITE with ARITHMETIC on the previous cell value (read-modify-write).
;;
;; KEY PATTERN: (write (cells) (?i) (+ (read (cells) ?i) 1))
;; This tests using the current cell value in an arithmetic expression as
;; the new write value — the increment-in-place pattern on array cells.
;;
;; Domain: a 3-cell score array; increment a cell if it is below the cap.

(define (domain score-counter)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 2)
        score - (number 0 5)
        board - (array 3 score)
    )

    (:functions
        (cells) - board
    )

    ;; Increment the score at slot ?i if it is strictly below 5.
    (:action inc
        :parameters (?i - slot)
        :precondition (< (read (cells) ?i) 5)
        :effect (write (cells) (?i) (+ (read (cells) ?i) 1))
    )
)
