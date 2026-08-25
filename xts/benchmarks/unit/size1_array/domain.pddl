;; Test: degenerate size-1 array (array 1 val).
;;
;; KEY PATTERN: the smallest possible array dimension.  Index 0 is the only
;;   valid index; the array.mk literal has a single element.  Verifies that
;;   the N-D infrastructure (store/select, frame ITE chain, ARRAY_CONSTANT
;;   default-fill) does not assume size >= 2 anywhere.

(define (domain size1)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val   - (number 0 9)
        one_t - (array 1 val)
    )

    (:functions
        (a) - one_t
    )

    (:action bump
        :parameters ()
        :precondition (= (read (a) (0)) 0)
        :effect (write (a) (0) 5)
    )
)
