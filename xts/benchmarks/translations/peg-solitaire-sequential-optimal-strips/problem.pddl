;; PDDL-XTS translation of pddl/test/peg-solitaire .../problem.pddl (minimal, 3-in-a-row).
;; occupied loc1,loc2 + free loc3  ->  pegs = [1,1,0].
;; Goal: loc1,loc2 free, loc3 occupied, move ended  ->  pegs = [0,0,1], moving = 0.

(define (problem minimal-pegsolitaire-xts)
    (:domain pegsolitaire-xts)
    (:init
        (= (pegs) (array.mk (1 1 0)))
        (= (moving) 0)
        (= (last_pos) 0)
    )
    (:goal (and
        (= (read (pegs) (0)) 0)
        (= (read (pegs) (1)) 0)
        (= (read (pegs) (2)) 1)
        (= (moving) 0)
    ))
)
