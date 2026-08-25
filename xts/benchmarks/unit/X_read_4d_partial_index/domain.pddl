;; BREAK TARGET: 4D array accessed with only 1 or 2 indices instead of 4.
;;
;; `tensor` is (array 2 2 2 2 val) — a 4D hypercube requiring 4 indices.
;; `partial3` uses (read (tensor) ?a ?b ?c) — only 3 indices provided,
;;   returning a 1D sub-array, not a scalar.  Assigning to a scalar is a type error.
;; `partial1` uses (read (tensor) ?a) — only 1 index, returning a 3D sub-array.
;;
;; Expected: arity/dimension error on both reads.

(define (domain X-read-4d-partial)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        d-idx   - (number 0 1)
        val     - (number 0 9)
        hyper   - (array 2 2 2 2 val)
    )

    (:functions
        (tensor) - hyper
        (result) - val
    )

    ;; Error: tensor[a][b][c] is a 1D sub-array, not a scalar.
    (:action partial3
        :parameters (?a - d-idx ?b - d-idx ?c - d-idx)
        :precondition ()
        :effect (assign (result) (read (tensor) ?a ?b ?c))
    )

    ;; Error: tensor[a] is a 3D sub-array, not a scalar.
    (:action partial1
        :parameters (?a - d-idx)
        :precondition ()
        :effect (assign (result) (read (tensor) ?a))
    )
)
