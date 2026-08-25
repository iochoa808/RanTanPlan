;; Test: 2D array whose element type is a set of USER OBJECTS.
;;
;; Covers: (array 2 3 tagset) where tagset = (set tag), tag is an object type.
;; Completes the matrix: 1D/2D × set{int} / set{obj} are now all covered.
;;
;; Domain: 2×3 label grid; propagate row-0 labels into row-1 via union.

(define (domain label-grid)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        row     - (number 0 1)
        col     - (number 0 2)
        tag     - object
        tagset  - (set tag)
        colset  - (set col)
        grid    - (array 2 3 tagset)
    )

    (:constants red blue green - tag)

    (:functions
        (labels)   - grid
        (active_c) - colset
    )

    ;; Union row-0 labels into row-1 for an active column.
    (:action propagate
        :parameters (?c - col)
        :precondition (member ?c (active_c))
        :effect (write ((labels) 1 ?c)
                       (union (read (labels) 1 ?c) (read (labels) 0 ?c)))
    )
)
