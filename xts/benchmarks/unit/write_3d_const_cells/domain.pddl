;; Test: constant-index writes on a 3D array — three-bracket IPAR cell names.
;;
;; KEY PATTERN: (write ((cube) 0 1 0) 4) with all three indices constant,
;; producing cell SVs like "cube[0][1][0]".  Extends two_explicit_writes_2d
;; to depth 3: the cell-name parser, nested store/select chains, and frame
;; axioms must all handle three index groups.

(define (domain write-3d-const)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val  - (number 0 9)
        cube_t - (array 2 2 2 val)
    )

    (:functions
        (cube) - cube_t
    )

    (:action poke_a
        :parameters ()
        :precondition (= (read (cube) (0) (1) (0)) 0)
        :effect (write ((cube) 0 1 0) 4)
    )

    (:action poke_b
        :parameters ()
        :precondition (and
            (= (read (cube) (0) (1) (0)) 4)
            (= (read (cube) (1) (0) (1)) 0)
        )
        :effect (write ((cube) 1 0 1) 9)
    )
)
