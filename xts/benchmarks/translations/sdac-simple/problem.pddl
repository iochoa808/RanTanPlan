;; PDDL-XTS translation of pddl/test/sdac-simple/problem.pddl.
(define (problem sdac-simple-1-xts)
    (:domain sdac-simple-xts)
    (:objects c0 - counter)
    (:init (= (value c0) 1) (is-target c0))
    (:goal (done))
)
