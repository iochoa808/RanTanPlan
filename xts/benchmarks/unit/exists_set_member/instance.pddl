;; bag={}.  Goal: detected.
;; Plan: fill (bag={7}), detect (7 ∈ bag satisfies exists i in [3..7]).

(define (problem sensor-01)
    (:domain sensor)

    (:init
        (= (bag) (set.mk ()))
    )

    (:goal (detected))
)
