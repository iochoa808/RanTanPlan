;; PDDL-XTS translation of pddl/test/parking .../problem.pddl (minimal-parking, swap).
(define (problem minimal-parking-xts)
    (:domain parking-xts)
    (:objects
        car1 car2 - car
        curb1 curb2 curb3 - curb
    )
    (:init
        (= (parked-on car1) curb1)
        (= (parked-on car2) curb2)
        (clear car1) (clear car2) (clear curb3)
    )
    (:goal (and (= (parked-on car1) curb2)
                (= (parked-on car2) curb1)))
)
