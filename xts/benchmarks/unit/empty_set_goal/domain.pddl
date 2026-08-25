;; Test: EMPTY-SET GOAL — (= (bag) (set.mk ())) as a top-level goal condition.
;;
;; KEY PATTERN: asserting that a set fluent is empty in the goal.
;;   Most goals use SetMember or whole-set literals with elements; this tests
;;   the zero-element case: the empty-set constructor (set.mk ()).
;;
;; Domain: bag = {1,2,3}; remove elements one by one.
;; Goal: bag = set.mk ()  (empty set).
;; Plan: remove(1), remove(2), remove(3)  — 3 steps.

(define (domain drain)
    (:requirements :sets :bounded-integers :typing)

    (:types
        val    - (number 1 3)
        valset - (set val)
    )

    (:functions
        (bag) - valset
    )

    (:action remove_elem
        :parameters (?n - val)
        :precondition (member ?n (bag))
        :effect (remove ?n (bag))
    )
)
