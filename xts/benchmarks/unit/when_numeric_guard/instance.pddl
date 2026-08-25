;; counter=3, cells=[0,0,0,0].  Goal: cells[0]=7, cells[1]=7.
;; Plan: inc, inc, stamp(0), stamp(1)  — 4 steps.
;;   inc×2 → counter=5; stamp guards now true → cells[0]=cells[1]=7.

(define (problem threshold-stamp-01)
    (:domain threshold-stamp)

    (:init
        (= (cells)   (array.mk (0 0 0 0)))
        (= (counter) 3)
    )

    (:goal (and
        (= (read (cells) 0) 7)
        (= (read (cells) 1) 7)
    ))
)
