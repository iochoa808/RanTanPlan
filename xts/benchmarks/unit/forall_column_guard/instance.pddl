;; board=[[0,1,2],[3,4,5],[6,7,8]], marker=4 (sitting on the diagonal cell [1][1]).
;; Column 0 = [0,3,6] contains no 4, so rotate_col_up(0) is applicable and makes
;; board[0][0]=3.  Goal: board[0][0]=3 — 1 step.

(define (problem forall-column-guard-01)
    (:domain forall-column-guard)

    (:init
        (= (board) (array.mk ((0 1 2) (3 4 5) (6 7 8))))
        (= (marker) 4)
        (= (probes) 0)
    )

    (:goal (= (read (board) 0 0) 3))
)
