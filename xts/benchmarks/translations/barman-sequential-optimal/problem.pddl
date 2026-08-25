;; PDDL-XTS translation of pddl/test/barman .../problem.pddl (simple-barman-prob).
;; level objects l0/l1/l2 + next -> bounded-int (shaker-level) starting at 0.
(define (problem simple-barman-xts)
    (:domain barman-xts)
    (:objects
        shaker1 - shaker
        left right - hand
        shot1 shot2 - shot
        ingredient1 ingredient2 - ingredient
        cocktail1 - cocktail
        dispenser1 dispenser2 - dispenser
    )
    (:init
        (ontable shaker1) (ontable shot1) (ontable shot2)
        (dispenses dispenser1 ingredient1) (dispenses dispenser2 ingredient2)
        (clean shaker1) (clean shot1) (clean shot2)
        (empty shaker1) (empty shot1) (empty shot2)
        (handempty left) (handempty right)
        (= (shaker-level shaker1) 0)
        (cocktail-part1 cocktail1 ingredient1)
        (cocktail-part2 cocktail1 ingredient2)
    )
    (:goal (contains shot1 cocktail1))
)
