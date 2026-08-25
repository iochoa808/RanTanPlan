;; Test: FORALL with a CONSTANT INTEGER RANGE — in both precondition and effect.
;;
;; KEY PATTERN:
;;   precondition: (forall (?i - (number 0 3)) (< (cell ?i) 4))
;;   effect:       (forall (?i - (number 0 3)) (assign (cell ?i) (+ (cell ?i) 1)))
;;
;; Uses scalar fluents (like pancake_bounded) to isolate the constant-range
;; feature from array-read interactions.  The bound here is a literal (3),
;; not an action parameter — the key difference from pancake_bounded.

(define (domain forall-const)
    (:requirements :typing :adl :fluents :bounded-integers)

    (:types
        idx - (number 0 3)
        val - (number 0 5)
    )

    (:functions
        (cell ?i - idx) - val
    )

    ;; Increment all four cells by 1, but only if every cell is currently < 4.
    (:action inc_all
        :parameters ()
        :precondition (forall (?i - (number 0 3)) (< (cell ?i) 4))
        :effect (forall (?i - (number 0 3))
                    (assign (cell ?i) (+ (cell ?i) 1)))
    )
)
