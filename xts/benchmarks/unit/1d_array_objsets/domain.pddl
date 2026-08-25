;; Test: 1D array whose element type is a set of USER OBJECTS.
;;
;; Covers: (array N tagset) where tagset = (set tag) and tag is an object type.
;; Previously only 2D arrays of object-sets were tested (labyrinth/domain2.pddl).
;;
;; Domain: 3 rooms, each holding a set of colour tags.
;; The gather action merges (unions) one room's tags into another.

(define (domain tag-row)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        slot   - (number 0 2)
        tag    - object
        tagset - (set tag)
        row    - (array 3 tagset)
    )

    (:constants red blue green - tag)

    (:functions
        (rooms) - row
    )

    ;; Union the tags of room ?src into room ?dst.
    (:action gather
        :parameters (?src - slot ?dst - slot)
        :precondition (not (= ?src ?dst))
        :effect (write (rooms) (?dst)
                       (union (read (rooms) ?dst) (read (rooms) ?src)))
    )
)
