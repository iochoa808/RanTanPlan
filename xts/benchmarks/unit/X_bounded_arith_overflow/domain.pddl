;; BREAK TARGET: arithmetic result that exceeds the declared type bound,
;; even though both individual operands are within their own declared ranges.
;;
;; A naive type checker might see:
;;   (a) : score [0..5]   ← in range
;;   (b) : score [0..5]   ← in range
;;   (+ (a) (b)) : score  ← WRONG — max sum is 10, bound is 5
;;
;; and accept the assignment because it does not statically analyse the
;; arithmetic range of the expression.  The sum can exceed the type's hi.
;;
;; The `combine` action has no preconditions restricting (a) + (b) <= 5,
;; so every grounding where a + b > 5 produces an out-of-range assignment.
;;
;; Expected: semantic error for the unconstrained add-overflow, OR the
;; planner should detect at grounding that the assign is out of range.
;;
;; Similarly, `multiply` computes (a) * (b) — max is 25, bound is 5.
;; And `negate` computes (- 0 (a)) — result is negative, below lo = 0.

(define (domain X-bounded-arith-overflow)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        score - (number 0 5)
    )

    (:functions
        (a) - score
        (b) - score
        (c) - score
    )

    ;; sum can reach 10 — out of [0,5] when both operands > 2
    (:action combine
        :parameters ()
        :precondition ()
        :effect (assign (c) (+ (a) (b)))
    )

    ;; product can reach 25 — out of [0,5] when both operands > 2
    (:action multiply
        :parameters ()
        :precondition ()
        :effect (assign (c) (* (a) (b)))
    )

    ;; negation: (- 0 (a)) is <= 0 always — below lo when (a) > 0
    (:action negate
        :parameters ()
        :precondition (> (a) 0)
        :effect (assign (c) (- 0 (a)))
    )
)
