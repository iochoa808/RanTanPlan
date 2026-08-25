;; SEMANTIC BREAK: a `set.mk` literal containing OBJECT constants used inside
;; a DOMAIN expression (here, a `member` precondition).
;;
;; Objects are declared in the PROBLEM, not the domain, so at domain-parse
;; time `a` and `b` are unknown.  The reader's set.mk handler then tries to
;; parse them as integers and crashes with `invalid literal for int(): 'a'`.
;;
;; This pins the limitation (object set.mk literals are problem-scoped only)
;; and flags that the failure mode is currently an ungraceful Python
;; ValueError rather than a clean domain error message.
;;
;; (Integer set.mk literals in domain expressions DO work — see sets_const,
;; which uses (set.mk (2 4 6)) in a precondition.)
;;
;; Expected: error — object set.mk literal cannot be resolved in the domain.

(define (domain X-set-mk-obj-in-domain)
    (:requirements :typing :sets :fluents)

    (:types
        item    - object
        itemset - (set item)
    )

    (:predicates (flagged ?x - item))

    (:functions (bag) - itemset)

    (:action flag
        :parameters (?x - item)
        :precondition (member ?x (set.mk (a b)))
        :effect (flagged ?x)
    )
)
