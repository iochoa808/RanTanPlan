;; board = [5, 3, 7, 2].  No agent is authorized yet.
;;
;; Goal: all four cells are 0.
;;
;; Plan: authorize(admin), wipe(admin)  — 2 steps.
;;   authorize(admin): sets (authorized admin).
;;   wipe(admin):      precondition (authorized admin) ✓
;;                     forall i in [0, 3]: (write (board) (i) 0)
;;                     → board[0]=0, board[1]=0, board[2]=0, board[3]=0.
;;
;; Correctness witness:
;;   The goal checks ALL four cells.  If the forall expansion misses even one
;;   index — e.g., only i=0 is written — three cells stay non-zero and the
;;   planner cannot reach the goal.  A correct 2-step plan exists iff the
;;   forall is fully expanded to four concrete WRITE effects.

(define (problem forall-obj-param-01)
    (:domain forall-obj-param)

    (:objects
        admin - agent
    )

    (:init
        (= (board) (array.mk (5 3 7 2)))
    )

    (:goal (and
        (= (read (board) 0) 0)
        (= (read (board) 1) 0)
        (= (read (board) 2) 0)
        (= (read (board) 3) 0)
    ))
)