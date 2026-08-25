;; cube = 2x2x2 zeros.
;; Goal: cube[0][1][0]=4, cube[1][0][1]=9, cube[1][1][1] untouched (=0).
;;
;; poke_b requires poke_a's value, forcing the order.
;;
;; Expected plan (2 steps): poke_a(), poke_b()

(define (problem write-3d-const-01)
    (:domain write-3d-const)

    (:init
        (= (cube) (array.mk ((0 0) (0 0)) ((0 0) (0 0))))
    )

    (:goal
        (and
            (= (read (cube) (0) (1) (0)) 4)
            (= (read (cube) (1) (0) (1)) 9)
            (= (read (cube) (1) (1) (1)) 0)
        )
    )
)
