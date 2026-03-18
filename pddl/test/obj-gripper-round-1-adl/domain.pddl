;; Object-fluent version of Gripper.
;; Replaces all predicates with object fluents:
;;   (at-robby ?r)       -> (robby-at) - room
;;   (at ?b ?r)          -> (ball-at ?b) - room
;;   (free ?g) / (carry ?b ?g) -> (holding ?g) - ball
;; Uses sentinel constants no-ball and no-room for "empty" states.

(define (domain obj-gripper-typed)
   (:requirements :typing :object-fluents)
   (:types room ball gripper)
   (:constants left right - gripper
               no-ball - ball
               no-room - room)
   (:functions (robby-at) - room
               (ball-at ?b - ball) - room
               (holding ?g - gripper) - ball)

   (:action move
       :parameters  (?from ?to - room)
       :precondition (= (robby-at) ?from)
       :effect (assign (robby-at) ?to))

   (:action pick
       :parameters (?obj - ball ?room - room ?gripper - gripper)
       :precondition  (and  (= (ball-at ?obj) ?room) (= (robby-at) ?room) (= (holding ?gripper) no-ball))
       :effect (and (assign (holding ?gripper) ?obj)
		    (assign (ball-at ?obj) no-room)))

   (:action drop
       :parameters  (?obj - ball ?room - room ?gripper - gripper)
       :precondition  (and  (= (holding ?gripper) ?obj) (= (robby-at) ?room))
       :effect (and (assign (ball-at ?obj) ?room)
		    (assign (holding ?gripper) no-ball))))
