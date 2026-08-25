;; Test: SET FLUENT EQUALITY IN GOAL — (= (basket) (set.mk (a b))) as goal.
;;
;; KEY PATTERN: the goal directly asserts a WHOLE SET LITERAL equality.
;;   Most tests use member / cardinality in goals.
;;   This exercises (= (fluent) (set.mk (...))) at the goal level with objects.
;;
;; Domain: add items to basket until it equals a target set literal.
;; Initial: basket={}.  Goal: basket = {item_a, item_b}.
;; Plan: add(item_a), add(item_b)  — 2 steps.

(define (domain basket)
    (:requirements :sets :typing)

    (:types
        item    - object
        itemset - (set item)
    )

    (:constants
        item_a item_b item_c - item
    )

    (:functions
        (basket) - itemset
    )

    (:action add
        :parameters (?x - item)
        :precondition (not (member ?x (basket)))
        :effect (add ?x (basket))
    )
)
