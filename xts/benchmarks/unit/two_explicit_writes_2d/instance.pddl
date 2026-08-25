;; board = 2x3 zeros.
;; Goal: board[0][1]=5 and board[1][2]=7, with board[1][0] untouched (=0).
;;
;; write_b requires write_a's value, so the only plan is:
;;   1. write_a()   board[0][1] := 5
;;   2. write_b()   board[1][2] := 7
;;
;; Expected plan (2 steps): write_a(), write_b()

(define (problem two-writes-2d-01)
    (:domain two-writes-2d)

    (:init
        (= (board) (array.mk (0 0 0) (0 0 0)))
    )

    (:goal
        (and
            (= (read (board) (0) (1)) 5)
            (= (read (board) (1) (2)) 7)
            (= (read (board) (1) (0)) 0)
        )
    )
)
