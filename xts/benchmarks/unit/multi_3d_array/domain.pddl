;; Test: two independent 3D array fluents in the same domain.
;;
;; KEY FEATURE: reading from one 3D array and writing to another.
;; Extends domain_multi_2d_array to the 3D case.
;;
;; Domain: copy a 2×2×2 source tensor into a destination tensor, cell by cell.

(define (domain dual-3d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        depth - (number 0 1)
        row   - (number 0 1)
        col   - (number 0 1)
        val   - (number 0 4)
        cube  - (array 2 2 2 val)
    )

    (:functions
        (src) - cube
        (dst) - cube
    )

    ;; Copy cell (?d, ?r, ?c) from src into dst.
    (:action copy
        :parameters (?d - depth ?r - row ?c - col ?v - val)
        :precondition (and
            (= (read (src) ?d ?r ?c) ?v)
            (not (= (read (dst) ?d ?r ?c) ?v))
        )
        :effect (write ((dst) ?d ?r ?c) ?v)
    )
)
