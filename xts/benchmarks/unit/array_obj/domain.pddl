;; Test domain: 1D array of user-type objects (color permutation).
;;
;; Covers: (array N color) where color is a PDDL object type.
;;
;; A 3-slot canvas holds a permutation of three color tokens.
;; Three swap actions cover every adjacent/non-adjacent pair of slots.
;; Constant indices are used so ArrayCompilatorPass produces simple
;; (= canvas_k ?c) equalities that the ObjectFluentIndex can ground.
;;
;; Element type: user object   Container: 1D array

(define (domain color-palette)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        color    - object
        slot_idx - (number 0 2)
        palette  - (array 3 color)
    )

    (:constants red green blue - color)

    (:functions
        (canvas) - palette
    )

    (:action swap_01
        :parameters (?ca ?cb - color)
        :precondition (and
            (= (read (canvas) 0) ?ca)
            (= (read (canvas) 1) ?cb)
        )
        :effect (and
            (write (canvas) (0) ?cb)
            (write (canvas) (1) ?ca)
        )
    )

    (:action swap_12
        :parameters (?ca ?cb - color)
        :precondition (and
            (= (read (canvas) 1) ?ca)
            (= (read (canvas) 2) ?cb)
        )
        :effect (and
            (write (canvas) (1) ?cb)
            (write (canvas) (2) ?ca)
        )
    )

    (:action swap_02
        :parameters (?ca ?cb - color)
        :precondition (and
            (= (read (canvas) 0) ?ca)
            (= (read (canvas) 2) ?cb)
        )
        :effect (and
            (write (canvas) (0) ?cb)
            (write (canvas) (2) ?ca)
        )
    )
)
