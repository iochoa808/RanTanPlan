;; Test: whole-array ASSIGN effect — (assign (arr) (array.mk ...)).
;;
;; Tests the empty-indices path in array_epc_index_ / ITE chain:
;;   arr_{t+1} = ite(action, ARRAY_CONSTANT(...), arr_t)
;; rather than a point-write store.  Analogous to (assign (set) (set.mk ...))
;; tested by setmk_effect, but for arrays.

(define (domain whole-replace)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val    - (number 0 9)
        tape_t - (array 3 val)
    )

    (:functions
        (tape) - tape_t
    )

    ;; Replace the entire tape with zeros.
    (:action zero_tape
        :parameters ()
        :effect (assign (tape) (array.mk (0 0 0)))
    )

    ;; Replace the entire tape with the target pattern.
    (:action load_pattern
        :parameters ()
        :effect (assign (tape) (array.mk (1 2 3)))
    )
)
