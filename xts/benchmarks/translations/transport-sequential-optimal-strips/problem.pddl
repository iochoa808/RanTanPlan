;; PDDL-XTS translation of the transport 3-node / 2-truck / 2-package instance.
;; Roads: 3<->1 and 3<->2 (the original symmetric road pairs).

(define (problem transport-xts-3nodes)
    (:domain transport-xts)
    (:objects
        city-loc-1 city-loc-2 city-loc-3 - location
        truck-1 truck-2 - vehicle
        package-1 package-2 - package
    )
    (:init
        (= (roads city-loc-3) (set.mk (city-loc-1 city-loc-2)))
        (= (roads city-loc-1) (set.mk (city-loc-3)))
        (= (roads city-loc-2) (set.mk (city-loc-3)))
        (= (vat truck-1) city-loc-3)
        (= (vat truck-2) city-loc-1)
        (= (capacity truck-1) 4)
        (= (capacity truck-2) 3)
        (pat package-1 city-loc-3)
        (pat package-2 city-loc-3)
    )
    (:goal (and (pat package-1 city-loc-2)
                (pat package-2 city-loc-2)))
)
