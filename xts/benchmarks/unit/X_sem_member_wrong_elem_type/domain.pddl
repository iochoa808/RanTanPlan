;; SEMANTIC BREAK: (member e set-fluent) where the type of e is incompatible
;; with the set's declared element type.
;;
;; SEMANTIC RULE: The element in a `member` test must have the same type as
;; the set's element type.  The membership question "is item_a in the set of
;; integers?" has no well-defined answer — item_a is not an integer.
;;
;; Case 1: `(member item_a (int_bag))` — item_a is object; int_bag ∈ set of level
;; Case 2: `(member 5 (obj_bag))` — 5 is an integer; obj_bag ∈ set of item
;; Case 3: `(member ?x (int_bag))` where ?x is declared as `item` (object)
;;
;; All three should be rejected as type errors.
;;
;; Note: these are in PRECONDITIONS, making the error a semantic type violation
;; on a condition formula, not an effect.  The planner might silently evaluate
;; membership as always-false (because the types never match) rather than
;; erroring — but the correct behaviour is to reject the domain.
;;
;; Expected: type error for `find_obj_in_int`, `find_int_in_obj`, `scan_wrong`.

(define (domain X-sem-member-wrong-elem-type)
    (:requirements :typing :sets :bounded-integers)

    (:types
        item    - object
        itemset - (set item)

        level   - (number 0 9)
        levelset - (set level)
    )

    (:constants item_a item_b - item)

    (:predicates
        (found)
    )

    (:functions
        (int_bag) - levelset
        (obj_bag) - itemset
    )

    ;; item_a is an object; int_bag requires integer elements
    (:action find_obj_in_int
        :parameters ()
        :precondition (member item_a (int_bag))
        :effect (found)
    )

    ;; 5 is an integer literal; obj_bag requires item objects
    (:action find_int_in_obj
        :parameters ()
        :precondition (member 5 (obj_bag))
        :effect (found)
    )

    ;; ?x is an item (object), int_bag requires integer elements
    (:action scan_wrong
        :parameters (?x - item)
        :precondition (member ?x (int_bag))
        :effect (found)
    )
)
