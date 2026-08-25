;; SEMANTIC BREAK: (decrease x delta) where delta alone exceeds the entire
;; declared type range, making underflow unconditional.
;;
;; `level` is (number 3 9) — lo = 3, hi = 9, range width = 6.
;; `(decrease (level) 50)` subtracts 50 regardless of the current value.
;; Even from the maximum (9), the result is 9 - 50 = -41, below lo = 3.
;;
;; SEMANTIC RULE: Unconditional underflow when hi - delta < lo.
;; Here: 9 - 50 = -41 < 3 → ALWAYS underflows.
;;
;; A second action `drain` uses delta = 7 (= range width + 1 = 6 + 1).
;; Even from hi = 9: 9 - 7 = 2 < lo = 3. Also guaranteed underflow.
;;
;; Both should be rejected. `safe_decrease` (delta = 1) is included for
;; contrast — it does not guarantee underflow.
;;
;; Expected: error/rejection for `blast_down` and `drain`.

(define (domain X-sem-decrease-underflow)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        level - (number 3 9)   ; lo=3, hi=9, width=6
    )

    (:functions
        (altitude) - level
    )

    ;; delta=50 >> width=6; 9-50=-41 < lo=3 — unconditional underflow
    (:action blast_down
        :parameters ()
        :precondition ()
        :effect (decrease (altitude) 50)
    )

    ;; delta=7 > width=6; even 9-7=2 < lo=3 — unconditional underflow
    (:action drain
        :parameters ()
        :precondition ()
        :effect (decrease (altitude) 7)
    )

    ;; delta=1 is safe — included for contrast
    (:action descend
        :parameters ()
        :precondition (> (altitude) 3)
        :effect (decrease (altitude) 1)
    )
)
