;; needed = {1, 2}, stocked = {1, 2, 3} → needed ⊆ stocked → ship fires.
;;
;; If needed were {1, 4} (4 not in stocked) the precondition would fail
;; and the problem would be UNSOLVABLE.
;;
;; Expected plan (1 step): ship()

(define (problem set-subseteq-01)
    (:domain set-subseteq)

    (:init
        (= (needed)  (set.mk (1 2)))
        (= (stocked) (set.mk (1 2 3)))
        (= (shipped) 0)
    )

    (:goal (= (shipped) 1))
)
