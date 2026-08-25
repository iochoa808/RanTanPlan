;; PDDL-XTS translation of pddl/test/logistics-strips-typed (logistics).
;; FEATURE: object fluents.
;;   - (in-city ?loc ?city) is a total function (each place is in exactly one
;;     city) -> object fluent (city-of ?p) - city.  DRIVE-TRUCK's same-city
;;     guard becomes (= (city-of ?from) (city-of ?to)), dropping the ?city
;;     parameter.
;;   - (at ?obj ?loc) and (in ?pkg ?veh) stay boolean: a package is either at a
;;     place OR inside a vehicle (a partial relation, not a total function).

(define (domain logistics-xts)
    (:requirements :typing :object-fluents)
    (:types
        truck airplane - vehicle
        package vehicle - physobj
        airport location - place
        city place physobj - object
    )

    (:predicates
        (at ?obj - physobj ?loc - place)
        (in ?pkg - package ?veh - vehicle)
    )
    (:functions
        (city-of ?p - place) - city
    )

    (:action LOAD-TRUCK
        :parameters (?pkg - package ?truck - truck ?loc - place)
        :precondition (and (at ?truck ?loc) (at ?pkg ?loc))
        :effect (and (not (at ?pkg ?loc)) (in ?pkg ?truck))
    )
    (:action LOAD-AIRPLANE
        :parameters (?pkg - package ?airplane - airplane ?loc - place)
        :precondition (and (at ?pkg ?loc) (at ?airplane ?loc))
        :effect (and (not (at ?pkg ?loc)) (in ?pkg ?airplane))
    )
    (:action UNLOAD-TRUCK
        :parameters (?pkg - package ?truck - truck ?loc - place)
        :precondition (and (at ?truck ?loc) (in ?pkg ?truck))
        :effect (and (not (in ?pkg ?truck)) (at ?pkg ?loc))
    )
    (:action UNLOAD-AIRPLANE
        :parameters (?pkg - package ?airplane - airplane ?loc - place)
        :precondition (and (in ?pkg ?airplane) (at ?airplane ?loc))
        :effect (and (not (in ?pkg ?airplane)) (at ?pkg ?loc))
    )
    (:action DRIVE-TRUCK
        :parameters (?truck - truck ?loc-from ?loc-to - place)
        :precondition (and (at ?truck ?loc-from)
                           (= (city-of ?loc-from) (city-of ?loc-to)))
        :effect (and (not (at ?truck ?loc-from)) (at ?truck ?loc-to))
    )
    (:action FLY-AIRPLANE
        :parameters (?airplane - airplane ?loc-from ?loc-to - airport)
        :precondition (at ?airplane ?loc-from)
        :effect (and (not (at ?airplane ?loc-from)) (at ?airplane ?loc-to))
    )
)
