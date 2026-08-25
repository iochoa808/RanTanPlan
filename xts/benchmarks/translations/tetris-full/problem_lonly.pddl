;; Isolate the right_l slide: ell0 orientation 1 (right+down arms) at corner
;; (0,0) -- footprint {(0,0),(0,1),(1,0)}. slide-l-o1-down should move the
;; corner to (1,0), footprint becomes {(1,0),(1,1),(2,0)}, freeing (0,0),(0,1).
;; Goal pins down the WHOLE board to that exact expected post-slide state, so
;; a plan can only be found if the effect set is exactly right (no missing or
;; extra cell writes survive).
(define (problem tetris-lonly)
    (:domain tetris-xts-full)
    (:objects
        ell0 - right_l
    )
    (:init
        (= (board) (array.mk ((1 1 0 0)
                               (1 0 0 0)
                               (0 0 0 0)
                               (0 0 0 0))))
        (= (lrow ell0) 0) (= (lcol ell0) 0)
        (= (lor  ell0) 1)
    )
    (:goal
        (= (board) (array.mk ((0 0 0 0)
                               (1 1 0 0)
                               (1 0 0 0)
                               (0 0 0 0))))
    )
)
