;; Test: one action writes to TWO different cells of the same array.
;;
;; Tests within-action store chaining: when a single effect block contains
;; two (write (arr) ...) effects, they must be chained as nested stores rather
;; than emitted as two separate conflicting arr_{t+1}=store(...) equalities.
;;
;; ITE chain result when stamp fires:
;;   arr_{t+1} = store(store(arr_t, 0, 1), 3, 2)
;; Cells 1 and 2 are untouched (frame semantics from store axiom).

(define (domain two-writes)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        idx   - (number 0 3)
        val   - (number 0 9)
        arr_t - (array 4 val)
    )

    (:functions
        (grid) - arr_t
    )

    (:action stamp
        :parameters ()
        :precondition (and
            (= (read (grid) 0) 0)
            (= (read (grid) 3) 0)
        )
        :effect (and
            (write (grid) (0) 1)
            (write (grid) (3) 2)
        )
    )
)
