;; PDDL-XTS translation of pddl/test/gripper-round-1-adl (gripper-typed).
;; FEATURE: object fluents.
;;   The robot is always in exactly one room, so the (at-robby ?r) predicate is
;;   a total function and becomes the object fluent (robby-at) - room.  Ball
;;   location and gripper state stay boolean: a ball is either at a room OR
;;   carried (a partial relation, not a total function), so it is not an object
;;   fluent.  (Mirrors the established obj-gripper-round-1-adl pattern.)

(define (domain gripper-typed-xts)
    (:requirements :typing :object-fluents)
    (:types room ball gripper)
    (:constants left right - gripper)

    (:predicates
        (at ?b - ball ?r - room)
        (carry ?b - ball ?g - gripper)
        (free ?g - gripper)
    )

    (:functions (robby-at) - room)

    (:action move
        :parameters (?from ?to - room)
        :precondition (= (robby-at) ?from)
        :effect (assign (robby-at) ?to)
    )

    (:action pick
        :parameters (?obj - ball ?room - room ?gripper - gripper)
        :precondition (and (at ?obj ?room) (= (robby-at) ?room) (free ?gripper))
        :effect (and (carry ?obj ?gripper)
                     (not (at ?obj ?room))
                     (not (free ?gripper)))
    )

    (:action drop
        :parameters (?obj - ball ?room - room ?gripper - gripper)
        :precondition (and (carry ?obj ?gripper) (= (robby-at) ?room))
        :effect (and (at ?obj ?room)
                     (not (carry ?obj ?gripper))
                     (free ?gripper))
    )
)
