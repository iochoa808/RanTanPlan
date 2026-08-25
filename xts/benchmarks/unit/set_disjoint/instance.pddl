;; owned = {item_a, item_b}, restricted = {item_c, item_d} → disjoint → proceed fires.
;;
;; If restricted contained item_a (owned), the precondition would fail.
;;
;; Expected plan (1 step): proceed()

(define (problem set-disjoint-01)
    (:domain set-disjoint)

    (:init
        (= (owned)      (set.mk (item_a item_b)))
        (= (restricted) (set.mk (item_c item_d)))
        (= (cleared)    0)
    )

    (:goal (= (cleared) 1))
)
