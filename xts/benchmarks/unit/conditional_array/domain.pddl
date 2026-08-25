;; Test: CONDITIONAL EFFECT on an array write.
;;
;; KEY PATTERN: (when (live ?c) (write (cells) (?s) 3))
;; No existing test exercises (when ...) wrapping an array-write effect.
;;
;; The predicate (live ?c) is over an object type, not the bounded-integer
;; slot type, because predicates over bounded integers are not valid PDDL.
;;
;; Domain: a 3-slot array; an activate action writes 3 to a slot only when
;; the given chip is live.

(define (domain cond-array)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        chip  - object
        slot  - (number 0 2)
        val   - (number 0 3)
        board - (array 3 val)
    )

    (:predicates
        (live ?c - chip)
    )

    (:functions
        (cells) - board
    )

    ;; Write 3 to slot ?s only if chip ?c is live.
    (:action activate
        :parameters (?c - chip ?s - slot)
        :precondition (= (read (cells) ?s) 0)
        :effect (when (live ?c)
                    (write (cells) (?s) 3))
    )
)
