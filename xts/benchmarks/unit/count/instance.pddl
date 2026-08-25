(define (problem party-lights-problem)
    (:domain party-lights)

    (:objects l1 l2 l3 - light)

    ;; l1 and l2 are on, l3 is off → count = 2, party can start immediately
    (:init
        (lit l1)
    )

    (:goal (party-on))
)
