;; PDDL-XTS translation of pddl/test/mprime/problem.pddl (mprime-x-1).
;; (eats a b) facts collected into (= (eats-set a) (set.mk (...))) per food.
(define (problem mprime-x-1-xts)
    (:domain mprime-xts)
    (:objects
        rice pear flounder okra pork lamb - food
        rest - pleasure
        hangover depression abrasion - pain
    )
    (:init
        (= (eats-set lamb) (set.mk (pork flounder)))
        (= (eats-set pork) (set.mk (okra lamb)))
        (= (eats-set okra) (set.mk (pear pork)))
        (= (eats-set rice) (set.mk (rice flounder pear)))
        (= (eats-set flounder) (set.mk (lamb rice)))
        (= (eats-set pear) (set.mk (okra rice)))
        (= (locale okra) 6) (= (locale pork) 5) (= (locale rice) 1)
        (= (locale pear) 2) (= (locale lamb) 3) (= (locale flounder) 4)
        (= (harmony rest) 3)
        (craves depression flounder)
        (craves abrasion pork)
        (craves rest pork)
        (craves hangover rice)
    )
    (:goal (craves abrasion rice))
)
