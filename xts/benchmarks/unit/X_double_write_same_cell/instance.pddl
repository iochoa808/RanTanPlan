;; cells[0] written twice: 3 then 7 — which wins?
;; cells[1] = 5 written twice: (5+1)=6 and (5+2)=7 — but if second read
;; sees the intermediate 6, result is (6+2)=8 instead of 7.
(define (problem X-double-write-same-cell-01)
    (:domain X-double-write-same-cell)
    (:init
        (= (cells) (array.mk (1 5 2)))
    )
    ;; With strict parallel semantics: cells[0]=7, cells[1]=7.
    ;; With sequential semantics:      cells[0]=7, cells[1]=8.
    (:goal (= (read (cells) 1) 7))
)
