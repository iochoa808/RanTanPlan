;; PDDL-XTS translation of pddl/test/rover/problem.pddl (roverprob1234).
;; Extends obj-rover problem with bounded ints + set fluent:
;;   (= (energy rover0) 50)            bounded int nrg_t [0,80]
;;   (= (recharges) 0)                 bounded int rch_t [0,10]
;;   12 (visible wX wY) predicates ->  4 (visible-to wX) set inits
;;     waypoint0 sees {waypoint1, waypoint2, waypoint3}  (and vice versa — all
;;     pairs mutually visible in this instance).
;; can_traverse stays boolean (3-arg, no clean set mapping).
;; Metric (minimize recharges) dropped; satisficing mode.

(define (problem roverprob1234-xts) (:domain rover-xts)
(:objects
    general - lander
    colour high_res low_res - mode
    rover0 - rover
    rover0store - store
    waypoint0 waypoint1 waypoint2 waypoint3 - waypoint
    camera0 - camera
    objective0 objective1 - objective)
(:init
    ;; Rover position, energy, recharge counter
    (= (location rover0) waypoint3)
    (= (energy rover0) 50)
    (= (recharges) 0)
    (available rover0)

    ;; Static functional structure (all static; StaticFluentPass folds these in)
    (= (lander_location general) waypoint0)
    (= (store_rover rover0store) rover0)
    (= (camera_rover camera0) rover0)
    (= (cal_target camera0) objective1)

    ;; Rover equipment and store state
    (equipped_for_soil_analysis rover0)
    (equipped_for_rock_analysis rover0)
    (equipped_for_imaging rover0)
    (empty rover0store)

    ;; Traversability: kept as boolean predicates (3-arg relation)
    (can_traverse rover0 waypoint3 waypoint0)
    (can_traverse rover0 waypoint0 waypoint3)
    (can_traverse rover0 waypoint3 waypoint1)
    (can_traverse rover0 waypoint1 waypoint3)
    (can_traverse rover0 waypoint1 waypoint2)
    (can_traverse rover0 waypoint2 waypoint1)

    ;; Visibility as sets: all four waypoints mutually visible in this instance.
    (= (visible-to waypoint0) (set.mk (waypoint1 waypoint2 waypoint3)))
    (= (visible-to waypoint1) (set.mk (waypoint0 waypoint2 waypoint3)))
    (= (visible-to waypoint2) (set.mk (waypoint0 waypoint1 waypoint3)))
    (= (visible-to waypoint3) (set.mk (waypoint0 waypoint1 waypoint2)))

    ;; Camera capabilities
    (supports camera0 colour)
    (supports camera0 high_res)

    ;; Objective visibility (from all waypoints in this instance)
    (visible_from objective0 waypoint0)
    (visible_from objective0 waypoint1)
    (visible_from objective0 waypoint2)
    (visible_from objective0 waypoint3)
    (visible_from objective1 waypoint0)
    (visible_from objective1 waypoint1)
    (visible_from objective1 waypoint2)
    (visible_from objective1 waypoint3)

    ;; Lander and communication
    (channel_free general)

    ;; Soil/rock samples and sunshine
    (at_soil_sample waypoint0)
    (in_sun waypoint0)
    (at_rock_sample waypoint1)
    (at_soil_sample waypoint2)
    (at_rock_sample waypoint2)
    (at_soil_sample waypoint3)
    (at_rock_sample waypoint3))

(:goal (and
    (communicated_soil_data waypoint2)
    (communicated_rock_data waypoint3)
    (communicated_image_data objective1 high_res)))
)
