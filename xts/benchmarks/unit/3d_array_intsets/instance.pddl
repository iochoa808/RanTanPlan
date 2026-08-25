;; cube[0][0][0]={1,2}, cube[0][0][1]={3}, all others={}.
;; Goal: 2 ∈ cube[1][0][0].
;; Plan: propagate(0,0)  — 1 step.

(define (problem code-cube-01)
    (:domain code-cube)

    (:init
        (= (cube) (array.mk ((((set.mk (1 2)) (set.mk (3)))
                               ((set.mk ())    (set.mk ())))
                              (((set.mk ())    (set.mk ()))
                               ((set.mk ())    (set.mk ()))))))
    )

    (:goal (member 2 (read (cube) 1 0 0)))
)
