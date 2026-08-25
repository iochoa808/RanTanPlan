;; SEMANTIC BREAK: an `array.mk` in an EFFECT supplies more elements than the
;; array's declared size.
;;
;; X_array_mk_overcount tests this for the :init block; this covers the
;; effect-value path (assign (a) (array.mk ...)), which is a different
;; construction site.  A size-3 array is assigned a 5-element literal.
;;
;; Expected: error — array literal element count does not match declared size.

(define (domain X-effect-array-mk-overcount)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val   - (number 0 9)
        arr_t - (array 3 val)
    )

    (:functions (a) - arr_t)

    (:action bad
        :parameters ()
        :effect (assign (a) (array.mk (1 2 3 4 5)))
    )
)
