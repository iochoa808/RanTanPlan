(define (domain petrobras)
    (:requirements :fluents :typing :negative-preconditions :conditional-effects :disjunctive-preconditions :sets)

    (:types
        location locatable - object
        ship cargo         - locatable
        port waiting_area platform - location

        cargo_set    - (set cargo)
        port_set     - (set port)
        platform_set - (set platform)
    )

    (:predicates
        (at_ ?sh - locatable ?where - location)
        (docked ?sh - ship ?where - location)
    )

    (:functions
        (current_fuel ?sh - ship)  - number
        (current_load ?sh - ship)  - number
        (fuel_capacity ?sh - ship) - number
        (load_capacity ?sh - ship) - number

        (current_docking_capacity ?p - location) - number
        (distance ?from - location ?to - location) - number
        (weight ?c - cargo) - number
        (total_fuel_used)   - number

        (cargo_on ?sh - ship) - cargo_set     ; cargo currently on the ship
        (refuel_ports)        - port_set      ; ports where refuelling is available
        (refuel_platforms)    - platform_set  ; platforms where refuelling is available
    )

    (:action sail
            :parameters (?sh - ship ?from - location ?to - location)
            :precondition (and
                (at_ ?sh ?from)
                (not (= ?from ?to))
                (not (docked ?sh ?from))
                (imply (= (current_load ?sh) 0)
                    (>= (current_fuel ?sh) (/ (distance ?from ?to) 5)))
                (imply (not (= (current_load ?sh) 0))
                    (>= (current_fuel ?sh) (/ (distance ?from ?to) 3))))
            :effect (and
                (at_ ?sh ?to)
                (not (at_ ?sh ?from))
                (when (= (current_load ?sh) 0)
                    (and (decrease (current_fuel ?sh) (/ (distance ?from ?to) 5))
                         (increase (total_fuel_used) (/ (distance ?from ?to) 5))))
                (when (not (= (current_load ?sh) 0))
                    (and (decrease (current_fuel ?sh) (/ (distance ?from ?to) 3))
                         (increase (total_fuel_used) (/ (distance ?from ?to) 3)))))
    )

    (:action dock
            :parameters (?sh - ship ?p - location)
            :precondition (and
                (> (current_docking_capacity ?p) 0)
                (not (docked ?sh ?p))
                (at_ ?sh ?p))
            :effect (and
                (decrease (current_docking_capacity ?p) 1)
                (docked ?sh ?p))
    )

    (:action undock
            :parameters (?sh - ship ?p - location)
            :precondition (docked ?sh ?p)
            :effect (and
                (increase (current_docking_capacity ?p) 1)
                (not (docked ?sh ?p)))
    )

    (:action load
            :parameters (?p - port ?sh - ship ?c - cargo)
            :precondition (and
                (docked ?sh ?p)
                (at_ ?c ?p)
                (>= (load_capacity ?sh) (+ (current_load ?sh) (weight ?c))))
            :effect (and
                (not (at_ ?c ?p))
                (add ?c (cargo_on ?sh))
                (increase (current_load ?sh) (weight ?c)))
    )

    (:action unload
            :parameters (?c - cargo ?p - platform ?sh - ship)
            :precondition (and
                (docked ?sh ?p)
                (member ?c (cargo_on ?sh)))
            :effect (and
                (decrease (current_load ?sh) (weight ?c))
                (remove ?c (cargo_on ?sh))
                (at_ ?c ?p))
    )

    ; Precondition guarantees fuel + 200 <= capacity, so increase is always safe.
    (:action refuel_at_port
            :parameters (?sh - ship ?l - port)
            :precondition (and
                (docked ?sh ?l)
                (member ?l (refuel_ports))
                (<= (+ (current_fuel ?sh) 200) (fuel_capacity ?sh)))
            :effect (increase (current_fuel ?sh) 200)
    )

    ; Precondition guarantees fuel + 100 <= capacity, so increase is always safe.
    (:action refuel_at_platform
            :parameters (?sh - ship ?l - platform)
            :precondition (and
                (docked ?sh ?l)
                (member ?l (refuel_platforms))
                (<= (+ (current_fuel ?sh) 100) (fuel_capacity ?sh)))
            :effect (increase (current_fuel ?sh) 100)
    )
)
