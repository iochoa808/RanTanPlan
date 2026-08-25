;; Any use of `blast` produces counter = 100, which is out of [0,5].
(define (problem X-sem-increase-overflow-01)
    (:domain X-sem-increase-overflow)
    (:init (= (counter) 0))
    (:goal (= (counter) 5))
)
