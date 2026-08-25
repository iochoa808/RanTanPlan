;; Test: 2D array whose element type is a USER OBJECT.
;;
;; Covers: (array 2 2 person) — 2D grid of PDDL objects.
;; Extends domain_array_obj (1D) to the 2D case.
;;
;; Domain: a 2×2 seating plan; swap the two occupants in the same row.

(define (domain seating-2d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        person  - object
        row     - (number 0 1)
        col     - (number 0 1)
        plan    - (array 2 2 person)
    )

    (:constants alice bob carol dave - person)

    (:functions
        (seats) - plan
    )

    ;; Swap the two occupants in row ?r.
    (:action swap_row
        :parameters (?r - row ?ca ?cb - person)
        :precondition (and
            (= (read (seats) ?r 0) ?ca)
            (= (read (seats) ?r 1) ?cb)
        )
        :effect (and
            (write ((seats) ?r 0) ?cb)
            (write ((seats) ?r 1) ?ca)
        )
    )
)
