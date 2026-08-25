;; PDDL-XTS translation of pddl/test/satellite-numeric-strips/problem.pddl (mini-satellite).
(define (problem mini-satellite-xts)
    (:domain satellite-numeric-xts)
    (:objects
        satellite0 - satellite
        instrument0 - instrument
        thermograph0 - mode
        GroundStation Target1 - direction
    )
    (:init
        (= (supports instrument0) (set.mk (thermograph0)))
        (= (cal-target instrument0) GroundStation)
        (= (on-board instrument0) satellite0)
        (power_avail satellite0)
        (= (pointing-at satellite0) Target1)
        (= (fuel satellite0) 20)
        (= (data_capacity satellite0) 50)
        (= (data Target1 thermograph0) 10)
        (= (slew_time GroundStation Target1) 5)
        (= (slew_time Target1 GroundStation) 5)
    )
    (:goal (have_image Target1 thermograph0))
)
