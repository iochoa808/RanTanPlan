;; a = [1, 2, 0]: needs cell 2 written to 3 before the whole-array precondition holds.
;; Goal: (done).
;;
;; Expected plan (2 steps): set2(), finish()

(define (problem whole-precond-01)
    (:domain whole-precond)

    (:init
        (= (a) (array.mk (1 2 0)))
    )

    (:goal (done))
)
