;; Test: EXISTS with a constant INTEGER RANGE in a precondition, with ARRAY READS.
;;
;; KEY PATTERN: (exists (?i - (number 0 3)) (> (read (cells) ?i) 5))
;;
;; Existential quantification over bounded-integer ranges appears to be
;; completely untested.  The body also contains an array read, making this
;; a combined exists+array test.
;;
;; Domain: a 4-slot array; detect whether any cell exceeds a threshold.

(define (domain exists-range)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx - (number 0 3)
        val - (number 0 9)
        arr - (array 4 val)
    )

    (:predicates
        (found_high)
    )

    (:functions
        (cells) - arr
    )

    ;; Mark found_high if any cell is > 5.
    (:action detect
        :parameters ()
        :precondition (and
            (not (found_high))
            (exists (?i - (number 0 3)) (> (read (cells) ?i) 5))
        )
        :effect (found_high)
    )

    ;; Bump a cell's value by 3 (to push it over the threshold).
    (:action boost
        :parameters (?i - idx)
        :precondition (not (found_high))
        :effect (write (cells) (?i) (+ (read (cells) ?i) 3))
    )
)
