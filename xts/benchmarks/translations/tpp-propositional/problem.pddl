;; PDDL-XTS translation of pddl/test/tpp-propositional/problem.pddl (TPP).
;; level0/level1 -> integer 0/1; (next level1 level0) -> the +/-1 transfer.
(define (problem TPP-prop-xts)
    (:domain tpp-prop-xts)
    (:objects
        goods1 - goods
        truck1 - truck
        market1 - market
        depot1 - depot
    )
    (:init
        (= (truck-at truck1) depot1)
        (= (connections depot1) (set.mk (market1)))
        (= (connections market1) (set.mk (depot1)))
        (= (ready-to-load goods1 market1) 0)
        (= (stored goods1) 0)
        (= (loaded goods1 truck1) 0)
        (= (on-sale goods1 market1) 1)
    )
    (:goal (= (stored goods1) 1))
)
