;; basket={0,2}, odds={}.  Goal: approved=1.
;; {0,2} ⊆ {0,2,4} ✓ and {} ∩ {1,3} = ∅ ✓ → approve fires immediately.
;; Plan: approve()  — 1 step.

(define (problem approval-01)
    (:domain approval)

    (:init
        (= (basket)   (set.mk (0 2)))
        (= (odds)     (set.mk ()))
        (= (approved) 0)
    )

    (:goal (= (approved) 1))
)
