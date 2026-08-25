;; Test: 4D array read/write.
;;
;; Covers: (array 2 2 2 2 val) — a 2×2×2×2 hypercube.
;; Tests that N-dimensional arrays (N > 3) parse and encode correctly.
;;
;; Domain: fill a cell of the hypercube from zero, or clear a non-zero cell.

(define (domain test-4d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        d0  - (number 0 1)
        d1  - (number 0 1)
        d2  - (number 0 1)
        d3  - (number 0 1)
        val - (number 0 3)
        hc  - (array 2 2 2 2 val)
    )

    (:functions
        (hypercube) - hc
    )

    (:action set
        :parameters (?a - d0 ?b - d1 ?c - d2 ?e - d3 ?v - val)
        :precondition (and
            (> ?v 0)
            (= (read (hypercube) ?a ?b ?c ?e) 0)
        )
        :effect (write ((hypercube) ?a ?b ?c ?e) ?v)
    )

    (:action clear
        :parameters (?a - d0 ?b - d1 ?c - d2 ?e - d3)
        :precondition (> (read (hypercube) ?a ?b ?c ?e) 0)
        :effect (write ((hypercube) ?a ?b ?c ?e) 0)
    )
)
