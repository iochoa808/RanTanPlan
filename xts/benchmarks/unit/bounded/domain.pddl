(define (domain thermostat)

    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        room - object

        ;; Temperature in [15, 25] degrees
        temp  - (number 15 25)

        ;; Setpoint in [18, 23] degrees (narrower range)
        setpoint - (number 18 23)
    )

    (:functions
        (current-temp ?r - room) - temp
        (target-temp  ?r - room) - setpoint
    )

    ;; Raise temperature by 1 degree (if below the room's target)
    (:action heat-up
        :parameters (?r - room)
        :precondition (< (current-temp ?r) (target-temp ?r))
        :effect (assign (current-temp ?r) (+ (current-temp ?r) 1))
    )

    ;; Lower temperature by 1 degree (if above the room's target)
    (:action cool-down
        :parameters (?r - room)
        :precondition (> (current-temp ?r) (target-temp ?r))
        :effect (assign (current-temp ?r) (- (current-temp ?r) 1))
    )
)
