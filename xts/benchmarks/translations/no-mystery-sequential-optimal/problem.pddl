;; PDDL-XTS translation of no-mystery minimal-transport.
(define (problem no-mystery-xts-minimal)
    (:domain no-mystery-xts)
    (:objects
        loc1 loc2 - location
        package1 - package
        truck1 - truck
    )
    (:init
        (= (connections loc1) (set.mk (loc2)))
        (= (connections loc2) (set.mk (loc1)))
        (= (tloc truck1) loc1)
        (= (fuel truck1) 2)
        (pat package1 loc1)
    )
    (:goal (pat package1 loc2))
)
