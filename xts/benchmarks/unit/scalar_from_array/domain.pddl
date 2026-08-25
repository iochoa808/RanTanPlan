;; Test: assign a scalar OBJECT-TYPED fluent from an array read.
;;
;; KEY PATTERN: (assign (picked) ?t) where ?t is bound by (= (read (stock) k) ?t)
;; This tests the object-fluent assignment path for values obtained from an array.
;; (Labyrinth v1 uses this pattern but it is not in the tests/ directory.)
;;
;; Domain: a 3-slot shelf of tokens; pick up the token at a specific slot.

(define (domain token-select)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 2)
        token - object
        shelf - (array 3 token)
    )

    (:constants tok_a tok_b tok_c - token)

    (:functions
        (stock)  - shelf    ; 3-slot array of token objects
        (picked) - token    ; currently held token
    )

    ;; Pick the token at slot 0.
    (:action pick_0
        :parameters (?t - token)
        :precondition (= (read (stock) 0) ?t)
        :effect (assign (picked) ?t)
    )

    (:action pick_1
        :parameters (?t - token)
        :precondition (= (read (stock) 1) ?t)
        :effect (assign (picked) ?t)
    )

    (:action pick_2
        :parameters (?t - token)
        :precondition (= (read (stock) 2) ?t)
        :effect (assign (picked) ?t)
    )
)
