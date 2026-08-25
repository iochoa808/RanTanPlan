;; width=2, height=1, area=0.  Goal: area=6.
;; Plan: set-height(3), compute  →  area = 2*3 = 6.

(define (problem area-01)
    (:domain area-calculator)

    (:init
        (= (width)  2)
        (= (height) 1)
        (= (area)   0)
    )

    (:goal (= (area) 6))
)
