;; Test: READ-MODIFY-WRITE (increment-in-place) on a 2D array cell.
;;
;; KEY PATTERN: (write ((board) ?i ?j) (+ (read (board) ?i ?j) 1))
;; Extends domain_fluent_index (1D) to the 2D case.
;;
;; Domain: a 2×3 score grid; increment a cell if below the cap.

(define (domain score-grid)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        row   - (number 0 1)
        col   - (number 0 2)
        score - (number 0 5)
        grid  - (array 2 3 score)
    )

    (:functions
        (board) - grid
    )

    ;; Increment the score at cell (?i, ?j) if strictly below 5.
    (:action inc
        :parameters (?i - row ?j - col)
        :precondition (< (read (board) ?i ?j) 5)
        :effect (write ((board) ?i ?j) (+ (read (board) ?i ?j) 1))
    )
)
