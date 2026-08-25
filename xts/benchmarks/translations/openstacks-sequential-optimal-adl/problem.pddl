;; PDDL-XTS translation of pddl/test/openstacks .../problem.pddl (simple-openstacks).
;; count objects n0..n3 + next-count + stacks-avail(n0) -> (= (stacks-avail) 0).
(define (problem simple-openstacks-xts)
    (:domain openstacks-adl-xts)
    (:objects
        o1 o2 - order
        p1 p2 - product
    )
    (:init
        (= (stacks-avail) 0)
        (waiting o1) (includes o1 p1)
        (waiting o2) (includes o2 p2)
    )
    (:goal (and (shipped o1) (shipped o2)))
)
