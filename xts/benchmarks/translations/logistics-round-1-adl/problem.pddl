;; PDDL-XTS translation of pddl/test/logistics-round-1-adl/problem.pddl (simple-logistics).
(define (problem simple-logistics-xts)
    (:domain logistics-adl-xts)
    (:objects
        package1 package2 - obj
        city1 city2 - city
        truck1 truck2 - truck
        plane1 - airplane
        city1-1 city2-1 - location
        city1-2 city2-2 - airport
    )
    (:init
        (= (city-of city1-1) city1) (= (city-of city1-2) city1)
        (= (city-of city2-1) city2) (= (city-of city2-2) city2)
        (at plane1 city1-2) (at truck1 city1-1) (at truck2 city2-1)
        (at package1 city1-1) (at package2 city2-1)
    )
    (:goal (and (at package1 city2-1) (at package2 city1-2)))
)
