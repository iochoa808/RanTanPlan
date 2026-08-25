;; BREAK TARGET: INLINE (number lo hi) used directly in :functions declaration
;;               without first defining a named type in :types.
;;
;; The guide requires bounded-integer types to be declared with a name in
;; :types before they can be used.  Writing:
;;   (:functions (f) - (number 0 5))
;; is not allowed — (number 0 5) must be named (e.g. `score - (number 0 5)`)
;; and `f` must be declared as `(f) - score`.
;;
;; Expected: parse/semantic error — inline anonymous type in :functions.

(define (domain X-inline-number-type)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:functions
        (f) - (number 0 5)
    )

    (:action bump
        :parameters ()
        :precondition (< (f) 5)
        :effect (assign (f) (+ (f) 1))
    )
)
