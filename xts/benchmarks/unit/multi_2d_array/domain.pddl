;; Test: two independent 2D ARRAY FLUENTS in the same domain.
;;
;; KEY FEATURE: reading from one 2D array and writing to another.
;; Extends domain_multi_array (1D) to the 2D case.
;;
;; Domain: copy a 2×2 source grid into a destination grid, cell by cell.

(define (domain dual-2d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        row  - (number 0 1)
        col  - (number 0 1)
        val  - (number 0 4)
        grid - (array 2 2 val)
    )

    (:functions
        (src) - grid
        (dst) - grid
    )

    ;; Copy cell (?r, ?c) from src into dst.
    (:action copy
        :parameters (?r - row ?c - col ?v - val)
        :precondition (and
            (= (read (src) ?r ?c) ?v)
            (not (= (read (dst) ?r ?c) ?v))
        )
        :effect (write ((dst) ?r ?c) ?v)
    )
)
