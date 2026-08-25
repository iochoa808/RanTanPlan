;; cells=[0,0,0,0].  Goal: cells[0]=1.
;; zero_cells(?lo) forall range (lo..3) with lo in [4,5] — always empty.
;; No write fires → cells stays 0 → goal unreachable → UNSOLVABLE.

(define (problem X-forall-lb-gt-ub-01)
    (:domain X-forall-lb-gt-ub)

    (:init
        (= (cells) (array.mk (0 0 0 0)))
    )

    (:goal (= (read (cells) 0) 1))
)
