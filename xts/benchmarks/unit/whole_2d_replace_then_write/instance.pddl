;; pad = 2x2 zeros.
;; Goal: the loaded pattern with one cell overwritten:
;;   pad[0][0]=1, pad[0][1]=2, pad[1][0]=3 (from load), pad[1][1]=9 (from touch).
;;
;; Expected plan (2 steps): load(), touch()

(define (problem whole-2d-replace-01)
    (:domain whole-2d-replace)

    (:init
        (= (pad) (array.mk (0 0) (0 0)))
    )

    (:goal
        (and
            (= (read (pad) (0) (0)) 1)
            (= (read (pad) (0) (1)) 2)
            (= (read (pad) (1) (0)) 3)
            (= (read (pad) (1) (1)) 9)
        )
    )
)
