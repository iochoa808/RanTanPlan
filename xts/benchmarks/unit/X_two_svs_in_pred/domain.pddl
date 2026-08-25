;; BREAK TARGET: two nested state-variable reads as arguments of the same
;; boolean predicate call.
;;
;; The spec (section 2.10, restrictions table) states:
;;   "Two nested SVs in the same predicate call — Not yet — rewrite one level"
;;
;; `(linked (read (board) ?r ?c) (read (board) ?r2 ?c2))` passes two
;; different array-read expressions into a single predicate.  Each read
;; expands into an ITE chain; composing two ITE chains inside one predicate
;; is not supported.
;;
;; Expected: semantic/compilation error for multiple nested SVs in one predicate.

(define (domain X-two-svs-in-pred)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx  - (number 0 2)
        node - object
        grid - (array 3 3 node)
    )

    (:constants n0 n1 n2 n3 n4 n5 n6 n7 n8 - node)

    (:predicates
        (linked ?a - node ?b - node)
    )

    (:functions
        (board) - grid
    )

    ;; both read-results passed to same predicate — must be rejected
    (:action check_link
        :parameters (?r ?c ?r2 ?c2 - idx)
        :precondition (linked (read (board) ?r ?c) (read (board) ?r2 ?c2))
        :effect ()
    )
)
