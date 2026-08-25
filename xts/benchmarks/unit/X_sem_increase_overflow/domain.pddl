;; SEMANTIC BREAK: (increase x delta) where delta alone exceeds the entire
;; declared type range, making overflow unconditional.
;;
;; `counter` is (number 0 5) — range width = 5.
;; `(increase (counter) 100)` adds 100 regardless of the current value.
;; Even from the minimum (0), the result would be 100, which is beyond hi=5.
;;
;; SEMANTIC RULE: An increase/decrease by a constant delta is out-of-range
;; when (current_value + delta) > hi for ALL values of current_value in [lo,hi].
;; Equivalently: lo + delta > hi → delta > hi - lo.
;; Here: 0 + 100 = 100 > 5 → ALWAYS overflows.
;;
;; Note: the interval evaluator in numeric_bounds_index.cpp CLAMPS values to
;; [lo, hi] after update instead of rejecting.  This test checks whether the
;; system detects statically guaranteed overflow rather than silently clamping
;; to 5 and proceeding as if the action were valid.
;;
;; Contrast with X_bounded_arith_overflow, which requires two operands to
;; combine at runtime; here the overflow is fully determined by the constant
;; delta alone.
;;
;; Expected: error/rejection for the `blast` action — not silent clamping.

(define (domain X-sem-increase-overflow)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        count - (number 0 5)   ; range width = 5
    )

    (:functions
        (counter) - count
    )

    ;; delta = 100 > hi - lo = 5; overflow is unconditional
    (:action blast
        :parameters ()
        :precondition ()
        :effect (increase (counter) 100)
    )

    ;; delta = 3 is safe when counter <= 2, unsafe when counter >= 3
    ;; (3 + 3 = 6 > 5); overflow depends on current value — included for
    ;; contrast so only `blast` is the definitively wrong action.
    (:action safe_increase
        :parameters ()
        :precondition (<= (counter) 2)
        :effect (increase (counter) 3)
    )
)
