;; BREAK TARGET: (assign (chain) (set.mk ((+ ?n 1) ?n))) in an effect.
;;
;; The spec (pitfalls table, section 7) explicitly states:
;;   "Arithmetic in set.mk (assign (f) (set.mk ((+ ?n 1))))" is NOT supported.
;;   Use (add (+ ?n 1) (f)) instead.
;;
;; The action `advance` tries to build a two-element literal from arithmetic
;; expressions.  This should be rejected rather than silently computing a
;; wrong set (e.g., using 0 or ignoring the expression).
;;
;; Expected: semantic/compilation error on the :effect of advance.

(define (domain X-set-mk-arithmetic-effect)
    (:requirements :sets :bounded-integers)

    (:types
        val     - (number 0 9)
        valset  - (set val)
    )

    (:functions
        (chain) - valset
    )

    ;; This action should be illegal: set.mk cannot hold arithmetic expressions
    (:action advance
        :parameters (?n - val)
        :precondition (and
            (< ?n 8)
            (member ?n (chain))
        )
        :effect (assign (chain) (set.mk ((+ ?n 1) ?n)))
    )
)
