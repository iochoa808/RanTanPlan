;; arr = [0, 0, 0].
;; Goal: every cell equals 1 — expressed with forall in the goal.
;;
;; The forall expands to:
;;   (= (read (arr) 0) 1) AND (= (read (arr) 1) 1) AND (= (read (arr) 2) 1)
;;
;; Expected plan (3 steps): fill(0), fill(1), fill(2)

(define (problem forall-goal-01)
    (:domain forall-goal)

    (:init
        (= (arr) (array.mk (0 0 0)))
    )

    (:goal
        (forall (?i - idx) (= (read (arr) ?i) 1))
    )
)
