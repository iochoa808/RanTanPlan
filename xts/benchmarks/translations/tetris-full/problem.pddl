;; 4x4 board.
;; square0 at (0,0), goal: reach (3,3).
;; domino0 (two_straight): back (0,2), front (0,3) -- horizontal, at top-right.
;; ell0 (right_l): orientation 1 (right+down arms), corner at (2,0);
;;   footprint = corner(2,0), right-arm(2,1), down-arm(3,0).
(define (problem tetris-full-demo)
    (:domain tetris-xts-full)
    (:objects
        square0 - one_square
        domino0 - two_straight
        ell0    - right_l
    )
    (:init
        (= (board) (array.mk ((1 0 1 1)
                               (0 0 0 0)
                               (1 1 0 0)
                               (1 0 0 0))))
        (= (srow square0) 0)
        (= (scol square0) 0)

        (= (drow1 domino0) 0) (= (dcol1 domino0) 2)
        (= (drow2 domino0) 0) (= (dcol2 domino0) 3)

        (= (lrow ell0) 2) (= (lcol ell0) 0)
        (= (lor  ell0) 1)
    )
    (:goal (and
        (= (read (board) 3 3) 1)
    ))
)
