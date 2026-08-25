;; PDDL-XTS translation of pddl/test/mprime (mystery-prime).
;; FEATURES: sets + bounded integers.
;;   - (eats ?n1 ?n2) binary static relation -> set fluent (eats-set ?n) - foodset.
;;   - (locale ?n) and (harmony ?v) numeric counters -> bounded ints (number 0 15).
;;   - (craves), (fears) stay boolean (dynamic many-to-many relations).

(define (domain mprime-xts)
    (:requirements :typing :sets :bounded-integers)
    (:types
        food emotion - object
        pleasure pain - emotion
        foodset - (set food)
        lvl - (number 0 15)
    )
    (:predicates
        (craves ?v - emotion ?n - food)
        (fears ?c - pain ?v - pleasure)
    )
    (:functions
        (eats-set ?n - food) - foodset
        (harmony ?v - emotion) - lvl
        (locale ?n - food) - lvl
    )

    (:action overcome
        :parameters (?c - pain ?v - pleasure ?n - food)
        :precondition (and (craves ?c ?n) (craves ?v ?n) (>= (harmony ?v) 1))
        :effect (and (not (craves ?c ?n)) (fears ?c ?v) (decrease (harmony ?v) 1))
    )
    (:action feast
        :parameters (?v - pleasure ?n1 ?n2 - food)
        :precondition (and (craves ?v ?n1) (member ?n2 (eats-set ?n1)) (>= (locale ?n1) 1))
        :effect (and (not (craves ?v ?n1)) (craves ?v ?n2) (decrease (locale ?n1) 1))
    )
    (:action succumb
        :parameters (?c - pain ?v - pleasure ?n - food)
        :precondition (and (fears ?c ?v) (craves ?v ?n))
        :effect (and (not (fears ?c ?v)) (craves ?c ?n) (increase (harmony ?v) 1))
    )
    (:action drink
        :parameters (?n1 ?n2 - food)
        :precondition (>= (locale ?n1) 1)
        :effect (and (decrease (locale ?n1) 1) (increase (locale ?n2) 1))
    )
)
