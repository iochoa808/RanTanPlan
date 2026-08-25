;; PDDL-XTS translation of pddl/test/satellite/problem.pddl (simple-satellite).
(define (problem simple-satellite-xts)
    (:domain satellite-xts)
    (:objects
        satellite0 - satellite
        instrument0 - instrument
        thermograph0 - mode
        GroundStation Target1 Target2 - direction
    )
    (:init
        (= (supports instrument0) (set.mk (thermograph0)))
        (= (cal-target instrument0) GroundStation)
        (= (on-board instrument0) satellite0)
        (power_avail satellite0)
        (= (pointing-at satellite0) GroundStation)
    )
    (:goal (and (have_image Target1 thermograph0)
                (have_image Target2 thermograph0)))
)
