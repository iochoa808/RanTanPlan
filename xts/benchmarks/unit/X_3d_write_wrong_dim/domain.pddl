;; Error test: 3D array accessed with only two indices.
;;
;; Mirrors X_read_2d_one_index but for the 3D case.
;; Both the read precondition and the write effect supply only 2 indices
;; for a 3-dimensional array — a dimension arity mismatch.

(define (domain test-3d-wrong-dim)
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
        (result) - val
    )

    ;; ERROR: (read (tensor) ?d ?r) supplies only 2 indices for a 3D array.
    ;; ERROR: (write ((tensor) ?d ?r) ?v) uses 2-index write on a 3D array.
    (:action bad-access
        :parameters (?d - depth ?r - row ?v - val)
        :precondition (= (read (tensor) ?d ?r) 0)
        :effect (write ((tensor) ?d ?r) ?v)
    )
)
