;; Test: add/remove effects with ARITHMETIC expressions as the set element.
;;
;; KEY PATTERNS:
;;   (member (+ ?n 1) (chain))   — membership test on computed value
;;   (add    (+ ?n 1) (chain))   — add computed value to set
;;   (remove (+ ?n 1) (chain))   — remove computed value from set
;;
;; Initial chain = {0, 4}.  Goal: 3 ∈ chain AND 4 ∉ chain.
;; Plan: step(0), step(1), step(2), trim(3)  — 4 steps.

(define (domain number-chain)
    (:requirements :bounded-integers :sets)

    (:types
        val     - (number 0 5)
        trim-n  - (number 0 4)   ;; n+1 ≤ 5, stays within val range
        valset  - (set val)
    )

    (:functions
        (chain) - valset
    )

    ;; step(n): add (n+1) when n present and (n+1) absent
    (:action step
        :parameters (?n - val)
        :precondition (and
            (< ?n 4)
            (member ?n (chain))
            (not (member (+ ?n 1) (chain)))
        )
        :effect (add (+ ?n 1) (chain))
    )

    ;; trim(n): remove (n+1) when both n and n+1 are present
    (:action trim
        :parameters (?n - trim-n)
        :precondition (and
            (member ?n (chain))
            (member (+ ?n 1) (chain))
        )
        :effect (remove (+ ?n 1) (chain))
    )
)
