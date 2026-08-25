;; PDDL-XTS translation of pddl/test/pathways-propositional/problem.pddl (Simple-Pathways).
;; level objects + next + num-subs facts replaced by the bounded-int budget (= (num-subs) 0).
(define (problem Simple-Pathways-xts)
    (:domain pathways-prop-xts)
    (:objects
        SP1 E2F13 pCAF p300 - simple
        SP1-E2F13 - complex
    )
    (:init
        (possible SP1) (possible E2F13) (possible pCAF) (possible p300)
        (association-reaction SP1 E2F13 SP1-E2F13)
        (association-reaction pCAF p300 pCAF-p300)
        (= (num-subs) 0)
    )
    (:goal (goal1))
)
