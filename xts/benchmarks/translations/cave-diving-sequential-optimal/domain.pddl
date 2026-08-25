;; PDDL-XTS translation of pddl/test/cave-diving-sequential-optimal.
;; FEATURES: bounded integers + object fluents + sets + ADL (forall/when kept).
;;   - (next-quantity ?q1 ?q2) chain + (capacity ?d ?q) predicate ->
;;     bounded-int fluent (cap ?d) : (number 0 3).  prepare-tank and pickup-tank
;;     decrement it (guard >= 1); drop-tank increments it (guard < 3).
;;   - (at-diver ?d ?l) + (at-surface ?d) -> object fluent (diver-at ?d) - location.
;;     surface-loc is a domain constant that represents "at the surface, not in
;;     water".  hire-diver leaves diver-at unchanged (already surface-loc); the
;;     enter-water / swim / decompress actions assign real or sentinel locations.
;;   - (connected ?l1 ?l2) static binary relation -> set fluent (connects ?l).
;;   - (next-tank) static successor chain stays as a predicate (already compact).
;;   - hire-diver forall/when (ADL) is preserved unchanged.
;;   - Cost metric dropped (satisficing).

(define (domain cave-diving-xts)
    (:requirements :typing :adl :bounded-integers :sets :object-fluents)
    (:types
        location diver tank - object
        locset - (set location)
        cap_t  - (number 0 3)
    )
    (:constants surface-loc - location)
    (:predicates
        (at-tank ?t - tank ?l - location)
        (in-storage ?t - tank)
        (full ?t - tank)
        (next-tank ?t1 ?t2 - tank)
        (available ?d - diver)
        (decompressing ?d - diver)
        (precludes ?d1 ?d2 - diver)
        (cave-entrance ?l - location)
        (holding ?d - diver ?t - tank)
        (have-photo ?l - location)
        (in-water)
    )
    (:functions
        (diver-at ?d - diver)    - location
        (connects ?l - location) - locset
        (cap ?d - diver)         - cap_t
    )

    ;; Hire a diver; preclude mutually exclusive divers.
    (:action hire-diver
        :parameters (?d1 - diver)
        :precondition (and (available ?d1) (not (in-water)))
        :effect (and (not (available ?d1))
                     (in-water)
                     (forall (?d2 - diver)
                         (when (precludes ?d1 ?d2) (not (available ?d2)))))
    )

    ;; Fill one tank from storage and give it to the diver (uses one capacity slot).
    (:action prepare-tank
        :parameters (?d - diver ?t1 ?t2 - tank)
        :precondition (and (= (diver-at ?d) surface-loc)
                           (not (available ?d))
                           (in-storage ?t1)
                           (next-tank ?t1 ?t2)
                           (>= (cap ?d) 1))
        :effect (and (not (in-storage ?t1))
                     (in-storage ?t2)
                     (full ?t1)
                     (holding ?d ?t1)
                     (decrease (cap ?d) 1))
    )

    ;; Diver enters the cave at the entrance location.
    (:action enter-water
        :parameters (?d - diver ?l - location)
        :precondition (and (= (diver-at ?d) surface-loc)
                           (not (available ?d))
                           (cave-entrance ?l))
        :effect (assign (diver-at ?d) ?l)
    )

    ;; Pick up a tank that was dropped at the diver's location.
    (:action pickup-tank
        :parameters (?d - diver ?t - tank ?l - location)
        :precondition (and (= (diver-at ?d) ?l)
                           (at-tank ?t ?l)
                           (>= (cap ?d) 1))
        :effect (and (not (at-tank ?t ?l))
                     (holding ?d ?t)
                     (decrease (cap ?d) 1))
    )

    ;; Drop a held tank at the diver's current location.
    (:action drop-tank
        :parameters (?d - diver ?t - tank ?l - location)
        :precondition (and (= (diver-at ?d) ?l)
                           (holding ?d ?t)
                           (< (cap ?d) 3))
        :effect (and (not (holding ?d ?t))
                     (at-tank ?t ?l)
                     (increase (cap ?d) 1))
    )

    ;; Swim to an adjacent cave location, consuming the held tank's air.
    (:action swim
        :parameters (?d - diver ?t - tank ?l1 ?l2 - location)
        :precondition (and (= (diver-at ?d) ?l1)
                           (holding ?d ?t)
                           (full ?t)
                           (member ?l2 (connects ?l1)))
        :effect (and (assign (diver-at ?d) ?l2)
                     (not (full ?t)))
    )

    ;; Photograph the current location, consuming the tank's air.
    (:action photograph
        :parameters (?d - diver ?l - location ?t - tank)
        :precondition (and (= (diver-at ?d) ?l)
                           (holding ?d ?t)
                           (full ?t))
        :effect (and (not (full ?t))
                     (have-photo ?l))
    )

    ;; Exit the water at the cave entrance; diver begins decompression.
    (:action decompress
        :parameters (?d - diver ?l - location)
        :precondition (and (= (diver-at ?d) ?l)
                           (cave-entrance ?l))
        :effect (and (assign (diver-at ?d) surface-loc)
                     (decompressing ?d)
                     (not (in-water)))
    )
)