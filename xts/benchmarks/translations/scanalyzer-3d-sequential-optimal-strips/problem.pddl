;; PDDL-XTS translation of pddl/test/scanalyzer .../problem.pddl (minimal-scanalyzer).
(define (problem minimal-scanalyzer-xts)
    (:domain scanalyzer3d-xts)
    (:objects
        car-a car-b - car
        seg-1 seg-2 - segment
    )
    (:init
        (CYCLE-2 seg-1 seg-2)
        (CYCLE-2-WITH-ANALYSIS seg-1 seg-2)
        (= (seg-of car-a) seg-1)
        (= (seg-of car-b) seg-2)
    )
    (:goal (and (analyzed car-a) (analyzed car-b)
                (= (seg-of car-a) seg-1) (= (seg-of car-b) seg-2)))
)
