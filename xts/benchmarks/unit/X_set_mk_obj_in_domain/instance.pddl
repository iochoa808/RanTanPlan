(define (problem X-set-mk-obj-in-domain-01)
    (:domain X-set-mk-obj-in-domain)

    (:objects a b c - item)

    (:init (= (bag) (set.mk (a))))

    (:goal (flagged b))
)
