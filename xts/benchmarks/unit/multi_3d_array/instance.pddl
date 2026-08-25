;; src = [[[1,2],[3,4]],[[0,1],[2,3]]].  dst = all zeros.
;; Goal: dst = src (whole-array equality).
;;
;; Plan: copy(d,r,c,v) for each non-zero cell — up to 7 steps.

(define (problem dual-3d-01)
    (:domain dual-3d)

    (:init
        (= (src) (array.mk (((1 2)(3 4))
                             ((0 1)(2 3)))))
        (= (dst) (array.mk (((0 0)(0 0))
                             ((0 0)(0 0)))))
    )

    (:goal
        (= (dst) (array.mk (((1 2)(3 4))
                             ((0 1)(2 3)))))
    )
)
