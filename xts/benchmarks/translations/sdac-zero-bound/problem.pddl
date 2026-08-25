;; PDDL-XTS translation of pddl/test/sdac-zero-bound/problem.pddl.
(define (problem sdac-zero-bound-1-xts)
    (:domain sdac-zero-bound-xts)
    (:objects c0 - counter)
    (:init (= (value c0) 0) (is-target c0))
    (:goal (done))
)
