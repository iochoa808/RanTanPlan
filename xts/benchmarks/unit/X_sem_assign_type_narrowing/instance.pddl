;; current=15, below setpoint's lo=18.  Goal requires target=15, which can
;; only be reached via sync_temp (the type-narrowing action).  If sync_temp
;; is correctly rejected, goal is unsolvable.  If silently accepted, target
;; becomes 15 and the goal is satisfied — reveals the type-narrowing bug.
(define (problem X-sem-assign-type-narrowing-01)
    (:domain X-sem-assign-type-narrowing)
    (:objects room1 - room)
    (:init
        (= (current room1)  15)
        (= (target  room1)  20)
    )
    (:goal (= (target room1) 15))
)
