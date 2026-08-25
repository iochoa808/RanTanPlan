;; src = [[3, 1], [4, 2]].  dst = [[0, 0], [0, 0]].
;; Goal: dst = src = [[3, 1], [4, 2]].
;;
;; Plan: copy(0,0,3), copy(0,1,1), copy(1,0,4), copy(1,1,2)  — 4 steps.

(define (problem dual-2d-01)
    (:domain dual-2d)

    (:init
        (= (src) (array.mk ((3 1)
                             (4 2))))
        (= (dst) (array.mk ((0 0)
                             (0 0))))
    )

    (:goal
        (= (dst) (array.mk ((3 1)
                             (4 2))))
    )
)
