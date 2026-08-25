;; PDDL-XTS translation of pddl/test/satellite-numeric-strips.
;; FEATURES: object fluents + sets + bounded integers.
;;   Same relational structure as the `satellite` translation (pointing-at /
;;   on-board / cal-target object fluents, supports set), but here the numeric
;;   resources DO bind, so they are KEPT as bounded ints:
;;     (fuel ?s) : (number 0 20)   (turn_to needs fuel >= slew_time)
;;     (data_capacity ?s) : (number 0 50)   (take_image needs capacity >= data)
;;   fuel-used / data-stored counters (metric only) are dropped.

(define (domain satellite-numeric-xts)
    (:requirements :typing :sets :object-fluents :bounded-integers :equality)
    (:types
        satellite direction instrument mode - object
        modeset - (set mode)
        fuelv - (number 0 20)
        datav - (number 0 50)
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
        (fuel ?s - satellite)        - fuelv
        (slew_time ?a ?b - direction) - fuelv
        (data_capacity ?s - satellite) - datav
        (data ?d - direction ?m - mode) - datav
    )

    (:action turn_to
        :parameters (?s - satellite ?d_new ?d_prev - direction)
        :precondition (and (= (pointing-at ?s) ?d_prev)
                           (>= (fuel ?s) (slew_time ?d_new ?d_prev)))
        :effect (and (assign (pointing-at ?s) ?d_new)
                     (decrease (fuel ?s) (slew_time ?d_new ?d_prev)))
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
                           (power_on ?i) (= (pointing-at ?s) ?d)
                           (>= (data_capacity ?s) (data ?d ?m)))
        :effect (and (decrease (data_capacity ?s) (data ?d ?m)) (have_image ?d ?m))
    )
)
