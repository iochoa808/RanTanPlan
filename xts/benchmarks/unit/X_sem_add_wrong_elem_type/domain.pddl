;; SEMANTIC BREAK: (add ?x (set-fluent)) where ?x is of an OBJECT type but
;; the set-fluent's element type is a BOUNDED INTEGER (and vice versa).
;;
;; SEMANTIC RULE: The element being added to (or removed from) a set must
;; have the same type as the set's declared element type.
;;
;; Case 1: `int_bag` is (set level) where level is (number 0 9).
;;   `(add item_a (int_bag))` — item_a is an object of type `item`, not an
;;   integer.  The element types are incompatible.
;;
;; Case 2: `obj_bag` is (set item) where item is an object type.
;;   `(add 5 (obj_bag))` — 5 is an integer literal, not an item object.
;;
;; Both should be caught as type errors at the semantic analysis phase.
;; A checker that only validates "set fluent exists" without checking element
;; type compatibility will silently accept these.
;;
;; Expected: type error for `put_obj_in_int_set` and `put_int_in_obj_set`.

(define (domain X-sem-add-wrong-elem-type)
    (:requirements :typing :sets :bounded-integers)

    (:types
        item    - object
        itemset - (set item)

        level   - (number 0 9)
        levelset - (set level)
    )

    (:constants item_a item_b - item)

    (:functions
        (int_bag) - levelset   ; elements must be level (integer 0..9)
        (obj_bag) - itemset    ; elements must be item (object)
    )

    ;; item_a is an object; int_bag expects integer elements — type mismatch
    (:action put_obj_in_int_set
        :parameters ()
        :precondition ()
        :effect (add item_a (int_bag))
    )

    ;; 5 is an integer; obj_bag expects item objects — type mismatch
    (:action put_int_in_obj_set
        :parameters ()
        :precondition ()
        :effect (add 5 (obj_bag))
    )

    ;; Correct actions — included to confirm that same-type add still works.
    (:action put_obj_correctly
        :parameters (?x - item)
        :precondition (not (member ?x (obj_bag)))
        :effect (add ?x (obj_bag))
    )

    (:action put_int_correctly
        :parameters (?n - level)
        :precondition (not (member ?n (int_bag)))
        :effect (add ?n (int_bag))
    )
)
