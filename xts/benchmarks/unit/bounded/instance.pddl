(define (problem thermostat-p1)
    (:domain thermostat)

    (:objects
        kitchen living-room - room
    )

    ;; kitchen starts cold (12), target 20
    ;; living-room starts hot (28), target 22
    (:init
        (= (current-temp kitchen)    16)
        (= (target-temp  kitchen)    20)
        (= (current-temp living-room) 25)
        (= (target-temp  living-room) 22)

    )

    (:goal
        (and
            (= (current-temp kitchen)    20)
            (= (current-temp living-room) 22)
        )
    )
)
