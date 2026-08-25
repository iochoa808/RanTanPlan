;; PDDL-XTS translation of pddl/test/markettrader/problem.pddl (minimal-trader).
(define (problem minimal-trader-xts)
    (:domain trader-xts)
    (:objects
        Market1 Market2 - market
        camel1 - camel
        Apple Banana - goods
    )
    (:init
        (= (price Apple Market1) 5)  (= (on-sale Apple Market1) 10)
        (= (price Banana Market1) 20) (= (on-sale Banana Market1) 0)
        (= (price Apple Market2) 15) (= (on-sale Apple Market2) 0)
        (= (price Banana Market2) 10) (= (on-sale Banana Market2) 5)
        (= (bought Apple) 0) (= (bought Banana) 0)
        (= (drive-cost Market1 Market2) 2) (= (drive-cost Market2 Market1) 2)
        (= (roads Market1) (set.mk (Market2)))
        (= (roads Market2) (set.mk (Market1)))
        (= (camel-at camel1) Market1)
        (= (cash) 20) (= (capacity) 2)
    )
    (:goal (>= (cash) 40))
)
