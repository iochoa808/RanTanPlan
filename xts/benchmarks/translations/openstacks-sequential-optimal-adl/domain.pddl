;; PDDL-XTS translation of pddl/test/openstacks-sequential-optimal-adl.
;; FEATURE: bounded integers (+ ADL forall/imply kept).
;;   The (stacks-avail ?s) + (next-count) count chain becomes a bounded-int
;;   gauge (stacks-avail) : (number 0 3): start-order uses a stack (-1),
;;   ship-order and open-new-stack free/add one (+1).
;;   Order/product status stay boolean; the forall/imply preconditions are kept.

(define (domain openstacks-adl-xts)
    (:requirements :typing :adl :bounded-integers)
    (:types
        order product - object
        scount - (number 0 3)
    )
    (:predicates
        (includes ?o - order ?p - product)
        (waiting ?o - order)
        (started ?o - order)
        (shipped ?o - order)
        (made ?p - product)
    )
    (:functions
        (stacks-avail) - scount
    )

    (:action make-product
        :parameters (?p - product)
        :precondition (and (not (made ?p))
                           (forall (?o - order) (imply (includes ?o ?p) (started ?o))))
        :effect (made ?p)
    )
    (:action start-order
        :parameters (?o - order)
        :precondition (and (waiting ?o) (>= (stacks-avail) 1))
        :effect (and (not (waiting ?o)) (started ?o) (decrease (stacks-avail) 1))
    )
    (:action ship-order
        :parameters (?o - order)
        :precondition (and (started ?o)
                           (forall (?p - product) (imply (includes ?o ?p) (made ?p)))
                           (< (stacks-avail) 3))
        :effect (and (not (started ?o)) (shipped ?o) (increase (stacks-avail) 1))
    )
    (:action open-new-stack
        :parameters ()
        :precondition (< (stacks-avail) 3)
        :effect (increase (stacks-avail) 1)
    )
)
