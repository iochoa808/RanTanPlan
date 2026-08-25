;; chain = {0, 4}.  Goal: 3 ∈ chain AND 4 ∉ chain.
;;
;; Plan: step(0), step(1), step(2), trim(3)  — 4 steps.

(define (problem chain-01)
    (:domain number-chain)

    (:init
        (= (chain) (set.mk (0 4)))
    )

    (:goal (and
        (member 3 (chain))
        (not (member 4 (chain)))
    ))
)
