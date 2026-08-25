;; Test: whole 2D array equality in a GOAL (not just init).
;;
;; KEY PATTERN: (= (board agent1) (array.mk (r0) (r1) (r2)))
;; Reuses domain2d.pddl (array 3 4 row, row = number 0 2).
;;
;; Init: all zeros.
;; Goal: board matches the literal pattern below.
;;
;; Plan: fill(agent1,0,0,1), fill(agent1,1,1,2), fill(agent1,2,2,1)  — 3 steps.

(define (problem test-2d-whole-goal)
    (:domain test-2d)

    (:objects agent1 - agent)

    (:init
        (= (board agent1) (array.mk (0 0 0 0) (0 0 0 0) (0 0 0 0)))
    )

    (:goal
        (= (board agent1) (array.mk (1 0 0 0)
                                    (0 2 0 0)
                                    (0 0 1 0)))
    )
)
