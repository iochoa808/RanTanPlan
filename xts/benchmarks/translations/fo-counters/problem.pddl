;; PDDL-XTS translation of pddl/test/fo-counters/problem.pddl (instance_2).
;; (max_int) folded into the (number 0 4) bound on (value).

(define (problem fn-counters-fo-xts-2)
    (:domain fn-counters-fo-xts)
    (:objects c0 c1 - counter)
    (:init
        (= (value c0) 0)
        (= (value c1) 0)
        (= (rate_value c0) 0)
        (= (rate_value c1) 0)
        (= (total-cost) 0)
    )
    (:goal (<= (+ (value c0) 1) (value c1)))
    (:metric minimize (total-cost))
)
