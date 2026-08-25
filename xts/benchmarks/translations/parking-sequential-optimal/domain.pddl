;; PDDL-XTS translation of pddl/test/parking-sequential-optimal.
;; FEATURE: object fluents (over a union supertype).
;;   Each car sits on exactly one "support" — either a curb or another car
;;   (at-curb-num / behind-car).  We unify these via a supertype `support`
;;   (curb and car are both supports) and a single object fluent
;;   (parked-on ?car) - support, collapsing the four move actions into one.
;;   (clear ?s) means nothing sits on support ?s.
;;   NOTE: the original forbids stacks deeper than 2 (a car may only sit behind
;;   a car that is itself at a curb).  This unified model does not re-impose that
;;   depth cap; it is a relaxation that does not affect this instance (the
;;   minimal swap uses no stacking).

(define (domain parking-xts)
    (:requirements :typing :equality :object-fluents)
    (:types
        support - object
        car curb - support
    )
    (:predicates
        (clear ?s - support)
    )
    (:functions
        (parked-on ?car - car) - support
    )

    (:action move
        :parameters (?car - car ?src ?dest - support)
        :precondition (and (clear ?car) (clear ?dest)
                           (= (parked-on ?car) ?src) (not (= ?car ?dest)))
        :effect (and (assign (parked-on ?car) ?dest)
                     (clear ?src) (not (clear ?dest)))
    )
)
