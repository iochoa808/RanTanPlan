;; PDDL-XTS translation of pddl/test/logistics-round-1-adl.
;; FEATURES: object fluents (+ ADL conditional effects kept).
;;   - (in-city ?l ?c) total function -> object fluent (city-of ?l) - city;
;;     drive-truck's same-city guard becomes (= (city-of ?from) (city-of ?to)).
;;   - (at ?x ?l), (in ?x ?veh), (loaded ?x) stay boolean (partial relations).
;;   The forall/when "carry loaded objects along" effects are preserved as-is.

(define (domain logistics-adl-xts)
    (:requirements :adl :object-fluents)
    (:types
        physobj - object
        obj vehicle - physobj
        truck airplane - vehicle
        location city - object
        airport - location
    )
    (:predicates
        (at ?x - physobj ?l - location)
        (in ?x - obj ?t - vehicle)
        (loaded ?x - physobj)
    )
    (:functions
        (city-of ?l - location) - city
    )

    (:action load
        :parameters (?obj - obj ?veh - vehicle ?loc - location)
        :precondition (and (at ?obj ?loc) (at ?veh ?loc) (not (loaded ?obj)))
        :effect (and (in ?obj ?veh) (loaded ?obj))
    )
    (:action unload
        :parameters (?obj - obj ?veh - vehicle ?loc - location)
        :precondition (and (in ?obj ?veh) (at ?veh ?loc))
        :effect (and (not (in ?obj ?veh)) (not (loaded ?obj)))
    )
    (:action drive-truck
        :parameters (?truck - truck ?loc-from ?loc-to - location)
        :precondition (and (at ?truck ?loc-from)
                           (= (city-of ?loc-from) (city-of ?loc-to)))
        :effect (and (at ?truck ?loc-to) (not (at ?truck ?loc-from))
                     (forall (?x - obj)
                         (when (in ?x ?truck)
                               (and (not (at ?x ?loc-from)) (at ?x ?loc-to)))))
    )
    (:action fly-airplane
        :parameters (?plane - airplane ?loc-from ?loc-to - airport)
        :precondition (at ?plane ?loc-from)
        :effect (and (at ?plane ?loc-to) (not (at ?plane ?loc-from))
                     (forall (?x - obj)
                         (when (in ?x ?plane)
                               (and (not (at ?x ?loc-from)) (at ?x ?loc-to)))))
    )
)
