;; Test: SET_SUBSETEQ operator in a precondition.
;;
;; Focused unit test for (subseteq A B): isolated from the multi-feature
;; complexity of sets2/3/4.  Verifies that the QF_AX enumeration encoding
;; for subseteq correctly gates an action.

(define (domain set-subseteq)
    (:requirements :typing :sets :bounded-integers)

    (:types
        item   - (number 0 4)
        bundle - (set item)
        flag   - (number 0 1)
    )

    (:functions
        (needed)   - bundle   ;; items the order requires
        (stocked)  - bundle   ;; items in the warehouse
        (shipped)  - flag
    )

    ;; Ship only if every required item is in stock.
    (:action ship
        :parameters ()
        :precondition (and
            (= (shipped) 0)
            (subset (needed) (stocked))
        )
        :effect (assign (shipped) 1)
    )
)
