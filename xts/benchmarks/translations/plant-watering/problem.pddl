;; PDDL-XTS translation of pddl/test/plant-watering/problem.pddl (instance_4_1).
;; (The original problem referenced a domain name that did not match its domain
;; file; here both use plant-watering-xts.)
(define (problem plant-watering-4-1-xts)
    (:domain plant-watering-xts)
    (:objects
        tap1 - tap
        agent1 - agent
        plant1 - plant
    )
    (:init
        (= (carrying) 10) (= (total_poured) 0) (= (total_loaded) 0) (= (poured plant1) 0)
        (= (x agent1) 3) (= (y agent1) 1)
        (= (x plant1) 2) (= (y plant1) 2)
        (= (x tap1) 3) (= (y tap1) 3)
    )
    (:goal (= (poured plant1) 4))
)
