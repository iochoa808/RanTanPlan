;; PDDL-XTS translation of pddl/test/counters/problem.pddl (instance_2).
;; (max_int) is gone — folded into the (number 0 4) bound in the domain.

(define (problem fn-counters-xts-2)
    (:domain fn-counters-xts)

    (:objects
        c0 c1 - counter
    )

    (:init
        (= (value c0) 0)
        (= (value c1) 0)
    )

    (:goal (<= (+ (value c0) 1) (value c1)))
)
