;; PDDL-XTS translation of pddl/test/city-car-sequential-optimal.
;; FEATURE: sets for static road-geometry relations.
;;   (same_line ?j1 ?j2) -> set fluent (line-neighbors ?j) - junctionset
;;       build_straight_oneway: (member ?xy_final (line-neighbors ?xy_initial))
;;   (diagonal ?j1 ?j2)  -> set fluent (diag-neighbors ?j) - junctionset
;;       build_diagonal_oneway: (member ?xy_final (diag-neighbors ?xy_initial))
;;
;; Car position stays as boolean predicates: a car is at_car_jun, at_car_road,
;; starting, or arrived — a PARTIAL union, not a total function, so no object fluent.
;; road_connect(?r, ?j1, ?j2) stays boolean: 3-arg + dynamic (build/destroy).
;; total-cost dropped: metric accumulator, not in goal, satisficing mode.
;;
;; destroy_road has (forall (?c1 - car) (when (at_car_road ?c1 ?r1) ...)) in
;; its effect — the same forall/when-in-effect pattern as cave-diving hire-diver.
;; This works correctly with the default IPAR,QR,GR pipeline.
;; Note: sets + forall/imply in PRECONDITIONS would segfault (see trucks note);
;; this domain uses forall/when only in an EFFECT, so it is safe.

(define (domain citycar-xts)
(:requirements :typing :equality :negative-preconditions :sets :conditional-effects)
(:types car junction garage road - object
        junctionset - (set junction))
(:predicates
    (at_car_jun ?c - car ?x - junction)
    (at_car_road ?c - car ?x - road)
    (starting ?c - car ?x - garage)
    (arrived ?c - car ?x - junction)
    (road_connect ?r1 - road ?xy - junction ?xy2 - junction)
    (clear ?xy - junction)
    (in_place ?x - road)
    (at_garage ?g - garage ?xy - junction))
(:functions
    (line-neighbors ?j - junction) - junctionset
    (diag-neighbors ?j - junction) - junctionset)

(:action move_car_in_road
    :parameters (?xy_initial - junction ?xy_final - junction ?machine - car ?r1 - road)
    :precondition (and (at_car_jun ?machine ?xy_initial)
                       (road_connect ?r1 ?xy_initial ?xy_final)
                       (in_place ?r1))
    :effect (and (clear ?xy_initial)
                 (at_car_road ?machine ?r1)
                 (not (at_car_jun ?machine ?xy_initial))))

(:action move_car_out_road
    :parameters (?xy_initial - junction ?xy_final - junction ?machine - car ?r1 - road)
    :precondition (and (at_car_road ?machine ?r1)
                       (clear ?xy_final)
                       (road_connect ?r1 ?xy_initial ?xy_final)
                       (in_place ?r1))
    :effect (and (at_car_jun ?machine ?xy_final)
                 (not (clear ?xy_final))
                 (not (at_car_road ?machine ?r1))))

(:action car_arrived
    :parameters (?xy_final - junction ?machine - car)
    :precondition (at_car_jun ?machine ?xy_final)
    :effect (and (clear ?xy_final)
                 (arrived ?machine ?xy_final)
                 (not (at_car_jun ?machine ?xy_final))))

(:action car_start
    :parameters (?xy_final - junction ?machine - car ?g - garage)
    :precondition (and (at_garage ?g ?xy_final)
                       (starting ?machine ?g)
                       (clear ?xy_final))
    :effect (and (not (clear ?xy_final))
                 (at_car_jun ?machine ?xy_final)
                 (not (starting ?machine ?g))))

(:action build_diagonal_oneway
    :parameters (?xy_initial - junction ?xy_final - junction ?r1 - road)
    :precondition (and (clear ?xy_final)
                       (not (in_place ?r1))
                       (member ?xy_final (diag-neighbors ?xy_initial)))
    :effect (and (road_connect ?r1 ?xy_initial ?xy_final)
                 (in_place ?r1)))

(:action build_straight_oneway
    :parameters (?xy_initial - junction ?xy_final - junction ?r1 - road)
    :precondition (and (clear ?xy_final)
                       (not (in_place ?r1))
                       (member ?xy_final (line-neighbors ?xy_initial)))
    :effect (and (road_connect ?r1 ?xy_initial ?xy_final)
                 (in_place ?r1)))

(:action destroy_road
    :parameters (?xy_initial - junction ?xy_final - junction ?r1 - road)
    :precondition (and (road_connect ?r1 ?xy_initial ?xy_final)
                       (in_place ?r1))
    :effect (and (not (in_place ?r1))
                 (not (road_connect ?r1 ?xy_initial ?xy_final))
                 (forall (?c1 - car)
                         (when (at_car_road ?c1 ?r1)
                               (and (not (at_car_road ?c1 ?r1))
                                    (at_car_jun ?c1 ?xy_initial))))))
)
