;; Hybrid object-fluent version of Gripper.
;; Uses an object fluent for robot location (always in exactly one room),
;; and boolean predicates for ball locations and gripper state (partial relations).

(define (domain obj-gripper-typed)
   (:requirements :typing :object-fluents)
   (:types room ball gripper)
   (:constants left right - gripper)
   (:predicates (at ?b - ball ?r - room)
                (carry ?b - ball ?g - gripper)
                (free ?g - gripper))
   (:functions (robby-at) - room)

   (:action move
       :parameters  (?from ?to - room)
       :precondition (and (= (robby-at) ?from))
       :effect (assign (robby-at) ?to))

   (:action pick
       :parameters (?obj - ball ?room - room ?gripper - gripper)
       :precondition  (and (at ?obj ?room)
                       (= (robby-at) ?room)
                       (free ?gripper))
       :effect (and (carry ?obj ?gripper)
                    (not (at ?obj ?room))
                    (not (free ?gripper))))

   (:action drop
       :parameters  (?obj - ball ?room - room ?gripper - gripper)
       :precondition  (and (carry ?obj ?gripper)
                       (= (robby-at) ?room))
       :effect (and (at ?obj ?room)
                    (not (carry ?obj ?gripper))
                    (free ?gripper))))
