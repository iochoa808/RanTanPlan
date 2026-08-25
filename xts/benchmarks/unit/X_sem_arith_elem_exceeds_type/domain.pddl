;; SEMANTIC BREAK: arithmetic expression used as a set element where the
;; computed value provably exceeds the element type's declared range.
;;
;; The guide (section 3.8) allows arithmetic expressions as set elements:
;;   (member (+ ?n 1) (chain))  — the expression must evaluate to the
;;   element type.
;;
;; SEMANTIC RULE: The expression (+ ?n 1) must evaluate within the
;; element type's range [lo, hi] for ALL groundings of ?n.
;;
;; Here `chain` is (set val) where val = (number 0 9).
;; Parameter ?n is of type `top` = (number 8 9) — hi = 9.
;; When ?n = 9: (+ 9 1) = 10, which exceeds val's hi = 9.
;;
;; So for EVERY grounding of ?n where ?n = 9, the `add` expression produces
;; an out-of-range element.  This is not a runtime surprise — it is a static
;; fact derivable from the parameter type alone.
;;
;; Contrast with a parameter of type (number 0 9): then (+ ?n 1) can reach
;; 10 only when ?n = 9, and a guard `(< ?n 9)` would make it safe.  But
;; `top = (number 8 9)` makes the guard impossible — it always overflows.
;;
;; Two actions:
;;   `step_overflow`: add (+ ?n 1) — always overflows when ?n = 9
;;   `check_overflow`: member (+ ?n 1) — same overflow in precondition
;;
;; Expected: type/range error for both actions.

(define (domain X-sem-arith-elem-exceeds-type)
    (:requirements :bounded-integers :sets)

    (:types
        val - (number 0 9)    ; element type range: [0, 9]
        top - (number 8 9)    ; ?n is always 8 or 9; (+ 9 1) = 10 > 9
        valset - (set val)
    )

    (:functions
        (chain) - valset
    )

    ;; ?n ∈ {8, 9}. When ?n = 9: (+ 9 1) = 10 > hi = 9 — out of element range.
    (:action step_overflow
        :parameters (?n - top)
        :precondition (member ?n (chain))
        :effect (add (+ ?n 1) (chain))
    )

    ;; Same overflow in precondition context.
    (:action check_overflow
        :parameters (?n - top)
        :precondition (member (+ ?n 1) (chain))
        :effect ()
    )
)
