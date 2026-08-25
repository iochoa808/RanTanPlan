;; Test: CONDITIONAL EFFECT on a set add/remove.
;;
;; KEY PATTERN: (when (not (quarantined ?x)) (add ?x (pool)))
;; No existing test exercises (when ...) wrapping a set-membership effect.
;;
;; Domain: a pool of items; items can only be admitted if not quarantined.

(define (domain cond-set)
    (:requirements :typing :adl :sets)

    (:types
        item    - object
        itemset - (set item)
    )

    (:predicates
        (quarantined ?x - item)
    )

    (:functions
        (pool) - itemset
    )

    ;; Admit item ?x into the pool, but only if it is not quarantined.
    (:action admit
        :parameters (?x - item)
        :precondition (not (member ?x (pool)))
        :effect (when (not (quarantined ?x))
                    (add ?x (pool)))
    )
)
