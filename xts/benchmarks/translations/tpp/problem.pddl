;; PDDL-XTS translation of pddl/test/tpp/problem.pddl (simple-tpp).
;; Prices and drive-costs dropped (metric-only); quantities bounded to [0,15].

(define (problem simple-tpp-xts)
    (:domain tpp-xts)
    (:objects
        market1 market2 - market
        depot0 - depot
        truck0 - truck
        goods0 - goods
    )
    (:init
        (= (on-sale goods0 market1) 5)
        (= (on-sale goods0 market2) 10)
        (= (loc-of truck0) depot0)
        (= (bought goods0) 0)
        (= (request goods0) 7)
    )
    (:goal (and (>= (bought goods0) (request goods0))
                (= (loc-of truck0) depot0)))
)
