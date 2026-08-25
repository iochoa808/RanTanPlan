;; stock = [tok_b, tok_a, tok_c].  picked = tok_a.
;; Goal: picked = tok_c (at slot 2).
;;
;; Plan: pick_2(tok_c)  — 1 step.

(define (problem token-select-01)
    (:domain token-select)

    (:init
        (= (stock)  (array.mk (tok_b tok_a tok_c)))
        (= (picked) tok_a)
    )

    (:goal (= (picked) tok_c))
)
