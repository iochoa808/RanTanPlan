;; cells=[5,7,3,9], active=[0,1,0,1].
;; Goal: cells[0]=0, cells[1]=0, cells[3]=0.
;; Plan: mark(0)  →  active=[1,1,0,1],  sweep  →  cells=[0,0,3,0].

(define (problem sweep-01)
    (:domain sweep)

    (:init
        (= (cells)  (array.mk (5 7 3 9)))
        (= (active) (array.mk (0 1 0 1)))
    )

    (:goal (and
        (= (read (cells) 0) 0)
        (= (read (cells) 1) 0)
        (= (read (cells) 3) 0)
    ))
)
