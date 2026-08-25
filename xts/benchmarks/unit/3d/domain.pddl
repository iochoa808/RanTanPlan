;; Test: basic 3D array read/write.
;;
;; Covers: (array 2 2 2 val) — 3D tensor with individual-cell fill and clear.
;; Extends domain_2d (2D) to the 3D case.
;;
;; Domain: a 2×2×2 integer tensor; fill an empty cell or clear a non-zero one.

(define (domain test-3d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        depth - (number 0 1)
        row   - (number 0 1)
        col   - (number 0 1)
        val   - (number 0 5)
        cube  - (array 2 2 2 val)
    )

    (:functions
        (tensor) - cube
    )

    ;; Write value ?v into cell (?d, ?r, ?c) if it is currently zero.
    (:action set
        :parameters (?d - depth ?r - row ?c - col ?v - val)
        :precondition (and
            (> ?v 0)
            (= (read (tensor) ?d ?r ?c) 0)
        )
        :effect (write ((tensor) ?d ?r ?c) ?v)
    )

    ;; Zero out a non-zero cell.
    (:action clear
        :parameters (?d - depth ?r - row ?c - col)
        :precondition (> (read (tensor) ?d ?r ?c) 0)
        :effect (write ((tensor) ?d ?r ?c) 0)
    )
)
