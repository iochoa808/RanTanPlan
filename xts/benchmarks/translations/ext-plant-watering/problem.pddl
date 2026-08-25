;; PDDL-XTS translation of pddl/test/ext-plant-watering/problem.pddl (instance_4_1).
;; plant2-4 are declared but unused by the goal; their coordinates are initialised
;; to (1,1) so the (number 1 4) range constraint is satisfied (the original left
;; them at the unbounded default 0).
(define (problem ext-plant-watering-4-1-xts)
    (:domain ext-plant-watering-xts)
    (:objects
        tap1 - tap
        agent1 - agent
        plant1 plant2 plant3 plant4 - plant
    )
    (:init
        (= (total_poured) 0) (= (total_loaded) 0) (= (water_reserve) 25)
        (= (x tap1) 3) (= (y tap1) 3)
        (= (x agent1) 3) (= (y agent1) 1) (= (carrying agent1) 0) (= (max_carry agent1) 5)
        (= (x plant1) 2) (= (y plant1) 2) (= (poured plant1) 0)
        (= (x plant2) 1) (= (y plant2) 1) (= (poured plant2) 0)
        (= (x plant3) 1) (= (y plant3) 1) (= (poured plant3) 0)
        (= (x plant4) 1) (= (y plant4) 1) (= (poured plant4) 0)
    )
    (:goal (and (= (poured plant1) 4) (= (total_poured) (total_loaded))))
)
