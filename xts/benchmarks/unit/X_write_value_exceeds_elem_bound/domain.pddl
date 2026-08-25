;; BREAK TARGET: write effect using a LITERAL value that statically exceeds
;; the element type's declared range.
;;
;; `cells` is (array 4 val) where val is (number 0 5).
;; `set_max` writes the literal 99 — beyond val's hi of 5.
;; `set_neg` writes the literal -7 — below val's lo of 0.
;;
;; Unlike X_bounded_arith_overflow (where the overflow requires evaluating
;; two runtime values), here the out-of-range value is a static literal,
;; making this a purely syntactic/semantic check with no runtime analysis
;; needed.
;;
;; The same mismatch is tested in a precondition comparison: comparing
;; `(read (cells) ?i)` against the literal 99 in `(= (read (cells) ?i) 99)`
;; cannot ever be true given the element type, so it should warn or error
;; about an unsatisfiable comparison.
;;
;; Expected: error for literal 99 and -7 as write values; warning/error
;; for unsatisfiable equality comparison against 99.

(define (domain X-write-value-exceeds-elem-bound)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 3)
        val   - (number 0 5)    ; element range [0, 5]
        store - (array 4 val)
    )

    (:functions
        (cells) - store
    )

    ;; 99 is a literal out of [0,5] for the element type — must be rejected
    (:action set_max
        :parameters (?i - slot)
        :precondition ()
        :effect (write (cells) (?i) 99)
    )

    ;; -7 is below lo=0 — must be rejected
    (:action set_neg
        :parameters (?i - slot)
        :precondition ()
        :effect (write (cells) (?i) -7)
    )

    ;; comparison against 99 is statically unsatisfiable for val in [0,5]
    (:action find_big
        :parameters (?i - slot)
        :precondition (= (read (cells) ?i) 99)
        :effect (write (cells) (?i) 5)
    )
)
