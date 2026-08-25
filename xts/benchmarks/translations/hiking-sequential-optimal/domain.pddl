;; PDDL-XTS translation of pddl/test/hiking-sequential-optimal.
;; FEATURE: object fluents.
;;   Four total-function "at" predicates become object fluents:
;;     (at_tent ?t ?p)   -> (tent-at ?t) - place
;;     (at_person ?x ?p) -> (person-at ?x) - place
;;     (at_car ?c ?p)    -> (car-at ?c) - place
;;     (walked ?cp ?p)   -> (walked-at ?cp) - place   ; how far a couple has hiked
;;   up/down (tent), partners and next stay boolean/relational.

(define (domain hiking-xts)
    (:requirements :typing :equality :object-fluents)
    (:types car tent person couple place)
    (:predicates
        (partners ?x1 - couple ?x2 ?x3 - person)
        (up ?x1 - tent)
        (down ?x1 - tent)
        (next ?x1 ?x2 - place)
    )
    (:functions
        (tent-at ?t - tent)     - place
        (person-at ?p - person) - place
        (car-at ?c - car)       - place
        (walked-at ?cp - couple) - place
    )

    (:action put_down
        :parameters (?x1 - person ?x3 - tent)
        :precondition (and (= (person-at ?x1) (tent-at ?x3)) (up ?x3))
        :effect (and (down ?x3) (not (up ?x3)))
    )
    (:action put_up
        :parameters (?x1 - person ?x3 - tent)
        :precondition (and (= (person-at ?x1) (tent-at ?x3)) (down ?x3))
        :effect (and (up ?x3) (not (down ?x3)))
    )
    (:action drive
        :parameters (?x1 - person ?x3 - place ?x4 - car)
        :precondition (= (person-at ?x1) (car-at ?x4))
        :effect (and (assign (person-at ?x1) ?x3) (assign (car-at ?x4) ?x3))
    )
    (:action drive_passenger
        :parameters (?x1 - person ?x3 - place ?x4 - car ?x5 - person)
        :precondition (and (= (person-at ?x1) (car-at ?x4))
                           (= (person-at ?x5) (car-at ?x4)) (not (= ?x1 ?x5)))
        :effect (and (assign (person-at ?x1) ?x3) (assign (car-at ?x4) ?x3)
                     (assign (person-at ?x5) ?x3))
    )
    (:action drive_tent
        :parameters (?x1 - person ?x3 - place ?x4 - car ?x5 - tent)
        :precondition (and (= (person-at ?x1) (car-at ?x4))
                           (= (tent-at ?x5) (car-at ?x4)) (down ?x5))
        :effect (and (assign (person-at ?x1) ?x3) (assign (car-at ?x4) ?x3)
                     (assign (tent-at ?x5) ?x3))
    )
    (:action drive_tent_passenger
        :parameters (?x1 - person ?x3 - place ?x4 - car ?x5 - tent ?x6 - person)
        :precondition (and (= (person-at ?x1) (car-at ?x4))
                           (= (tent-at ?x5) (car-at ?x4)) (down ?x5)
                           (= (person-at ?x6) (car-at ?x4)) (not (= ?x1 ?x6)))
        :effect (and (assign (person-at ?x1) ?x3) (assign (car-at ?x4) ?x3)
                     (assign (tent-at ?x5) ?x3) (assign (person-at ?x6) ?x3))
    )
    (:action walk_together
        :parameters (?x1 - tent ?x2 - place ?x3 - person ?x4 - place ?x5 - person ?x6 - couple)
        :precondition (and (= (tent-at ?x1) ?x2) (up ?x1)
                           (= (person-at ?x3) ?x4) (next ?x4 ?x2)
                           (= (person-at ?x5) ?x4) (not (= ?x3 ?x5))
                           (= (walked-at ?x6) ?x4) (partners ?x6 ?x3 ?x5))
        :effect (and (assign (person-at ?x3) ?x2) (assign (person-at ?x5) ?x2)
                     (assign (walked-at ?x6) ?x2))
    )
)
