;; domino0 starts horizontal: back(1,0) front(1,1). A "turn" move should pivot
;; around the front cell (1,1) into a vertical orientation: slide-two-up from
;; front(1,1) -> new front (0,1), new back = old front (1,1). Final footprint
;; {(0,1),(1,1)}, old back (1,0) freed. This only works if the front cell can
;; move in ANY direction, not just continuing the original horizontal axis.
(define (problem tetris-turn)
    (:domain tetris-xts-full)
    (:objects domino0 - two_straight)
    (:init
        (= (board) (array.mk ((0 0 0 0)
                               (1 1 0 0)
                               (0 0 0 0)
                               (0 0 0 0))))
        (= (drow1 domino0) 1) (= (dcol1 domino0) 0)
        (= (drow2 domino0) 1) (= (dcol2 domino0) 1)
    )
    (:goal
        (= (board) (array.mk ((0 1 0 0)
                               (0 1 0 0)
                               (0 0 0 0)
                               (0 0 0 0))))
    )
)
