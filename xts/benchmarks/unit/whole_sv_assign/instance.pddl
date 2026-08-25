;; src = [4, 5, 6], dst = [0, 0, 0].
;; Goal: dst[0]=4, dst[2]=6 (i.e. dst became a copy of src).
;;
;; Expected plan (1 step): copy_all()

(define (problem sv-assign-01)
    (:domain sv-assign)

    (:init
        (= (src) (array.mk (4 5 6)))
        (= (dst) (array.mk (0 0 0)))
    )

    (:goal
        (and
            (= (read (dst) (0)) 4)
            (= (read (dst) (2)) 6)
        )
    )
)
