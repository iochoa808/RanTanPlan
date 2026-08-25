;; PDDL-XTS translation of pddl/test/warehouse/problem.pddl (warehouse-problem).
;; Same instance: bin0={item0,item1} cap2 (full), bin1={item2} cap2,
;; bin2={item3} cap3. A bin counts as full once cardinality >= 2; goal is
;; reached once at least 2 of the 3 bins are full.
(define (problem warehouse-problem-xts)
    (:domain warehouse-xts)
    (:init
        (= (bins) (array.mk ((set.mk (0 1)) (set.mk (2)) (set.mk (3)))))
        (= (capacity) (array.mk (2 2 3)))
    )
    (:goal (>= (count
        (>= (cardinality (read (bins) 0)) 2)
        (>= (cardinality (read (bins) 1)) 2)
        (>= (cardinality (read (bins) 2)) 2)
    ) 2))
)