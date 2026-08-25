;; src = [3, 1, 4].  dst = [0, 0, 0].
;; Goal: dst = src = [3, 1, 4].
;;
;; Plan: copy_0(3), copy_1(1), copy_2(4)  — 3 steps.

(define (problem dual-array-01)
    (:domain dual-array)

    (:init
        (= (src) (array.mk (3 1 4)))
        (= (dst) (array.mk (0 0 0)))
    )

    (:goal
        (= (dst) (array.mk (3 1 4)))
    )
)
