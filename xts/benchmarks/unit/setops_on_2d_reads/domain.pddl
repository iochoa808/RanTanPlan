;; Test: SET PREDICATES applied to values READ from a 2D array.
;;
;; KEY PATTERNS:
;;   (>= (cardinality (read (cells) ?r ?c)) 2)   — cardinality on a 2D-read set
;;   (write ((cells) ?r ?c) (intersect ...))       — intersect-write to 2D cell
;;
;; Extends domain_setops_on_reads (1D) to the 2D case.
;;
;; Domain: 2×3 grid of int-sets; count cells with >= 2 elements.

(define (domain grid-inspector)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        row   - (number 0 1)
        col   - (number 0 2)
        val   - (number 0 4)
        vset  - (set val)
        grid  - (array 2 3 vset)
        count - (number 0 6)
    )

    (:functions
        (cells) - grid
        (big)   - count
    )

    ;; Increment counter for a cell that has >= 2 elements.
    (:action count_big
        :parameters (?r - row ?c - col)
        :precondition (and
            (< (big) 6)
            (>= (cardinality (read (cells) ?r ?c)) 2)
        )
        :effect (assign (big) (+ (big) 1))
    )
)
