;; PDDL-XTS translation of pddl/test/satellite.
;; FEATURES: object fluents + sets.
;;   - (pointing ?s ?d) total function     -> object fluent (pointing-at ?s) - direction.
;;   - (on_board ?i ?s) total function      -> object fluent (on-board ?i) - satellite.
;;   - (calibration_target ?i ?d) function  -> object fluent (cal-target ?i) - direction.
;;   - (supports ?i ?m) binary relation     -> set fluent (supports ?i) - modeset.
;;   power/calibration stay boolean.
;;   Resource numerics (fuel/slew_time, data_capacity/data) are DROPPED: in this
;;   instance the limits (fuel 50, capacity 100) never bind, and bounding them
;;   would only inflate the encoding.  Logical structure is preserved.

(define (domain satellite-xts)
    (:requirements :typing :sets :object-fluents :equality)
    (:types
        satellite direction instrument mode - object
        modeset - (set mode)
    )
    (:predicates
        (power_avail ?s - satellite)
        (power_on ?i - instrument)
        (calibrated ?i - instrument)
        (have_image ?d - direction ?m - mode)
    )
    (:functions
        (pointing-at ?s - satellite) - direction
        (on-board ?i - instrument)   - satellite
        (cal-target ?i - instrument) - direction
        (supports ?i - instrument)   - modeset
    )

    (:action turn_to
        :parameters (?s - satellite ?d_new ?d_prev - direction)
        :precondition (and (= (pointing-at ?s) ?d_prev) (not (= ?d_new ?d_prev)))
        :effect (assign (pointing-at ?s) ?d_new)
    )
    (:action switch_on
        :parameters (?i - instrument ?s - satellite)
        :precondition (and (= (on-board ?i) ?s) (power_avail ?s))
        :effect (and (power_on ?i) (not (calibrated ?i)) (not (power_avail ?s)))
    )
    (:action switch_off
        :parameters (?i - instrument ?s - satellite)
        :precondition (and (= (on-board ?i) ?s) (power_on ?i))
        :effect (and (not (power_on ?i)) (power_avail ?s))
    )
    (:action calibrate
        :parameters (?s - satellite ?i - instrument ?d - direction)
        :precondition (and (= (on-board ?i) ?s) (= (cal-target ?i) ?d)
                           (= (pointing-at ?s) ?d) (power_on ?i))
        :effect (calibrated ?i)
    )
    (:action take_image
        :parameters (?s - satellite ?d - direction ?i - instrument ?m - mode)
        :precondition (and (calibrated ?i) (= (on-board ?i) ?s) (member ?m (supports ?i))
                           (power_on ?i) (= (pointing-at ?s) ?d))
        :effect (have_image ?d ?m)
    )
)
