;; PDDL-XTS translation of pddl/test/logistics-strips-typed/problem.pddl (logistics-4-0).
;; (in-city ...) facts become (= (city-of place) city) assignments.

(define (problem logistics-4-0-xts)
    (:domain logistics-xts)
    (:objects
        apn1 - airplane
        apt1 apt2 - airport
        pos2 pos1 - location
        cit2 cit1 - city
        tru2 tru1 - truck
        obj23 obj22 obj21 obj13 obj12 obj11 - package
    )
    (:init
        (at apn1 apt2) (at tru1 pos1) (at obj11 pos1)
        (at obj12 pos1) (at obj13 pos1) (at tru2 pos2) (at obj21 pos2) (at obj22 pos2)
        (at obj23 pos2)
        (= (city-of pos1) cit1) (= (city-of apt1) cit1)
        (= (city-of pos2) cit2) (= (city-of apt2) cit2)
    )
    (:goal (and (at obj11 apt1) (at obj23 pos1) (at obj13 apt1) (at obj21 pos1)))
)
