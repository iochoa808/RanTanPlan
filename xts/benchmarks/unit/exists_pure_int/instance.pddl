;; threshold=0.  Goal: done.
;; inc() → threshold=1 → exists(i=0: 1>0) ✓ → unlock() → done.
;; Plan: inc(), unlock()  — 2 steps.

(define (problem exists-pure-01)
    (:domain exists-pure)

    (:init
        (= (threshold) 0)
    )

    (:goal (done))
)
