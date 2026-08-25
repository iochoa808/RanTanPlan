;; Test: whole-array ASSIGN where the value is ANOTHER array fluent (SV-to-SV),
;; not an array.mk literal.
;;
;; KEY PATTERN: (assign (dst) (src)) copies an entire array in one effect.
;;   In the frame-axiom ITE chain this is an empty-indices replacement record
;;   whose val_id is a STATE_VARIABLE (the src array), not an ARRAY_CONSTANT.
;;   whole_array_replace / whole_2d_replace cover array.mk-literal replacement;
;;   this covers the SV-valued replacement, which exercises a different
;;   convert_expr_id_to_z3 path (the value side is a live array variable,
;;   so both src and dst must resolve to z3::Array constants and be equated).

(define (domain sv-assign)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val   - (number 0 9)
        arr_t - (array 3 val)
    )

    (:functions
        (src) - arr_t
        (dst) - arr_t
    )

    ;; Copy the whole src array into dst.
    (:action copy_all
        :parameters ()
        :effect (assign (dst) (src))
    )
)
