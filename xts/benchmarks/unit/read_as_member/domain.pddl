;; Test: integer value read from an array used as a set-membership element.
;;
;; KEY PATTERN: (member ?n (called)) where ?n = (read (tiles) ?p)
;; This is the inverse of labyrinth's pattern (member dir (read set_array i)).
;; Here the ARRAY stores the element value; the SET stores the membership oracle.
;;
;; Domain: a 4-slot bingo board of integers; a "called" set of drawn numbers.
;; The mark action fires when the tile at a position was drawn.

(define (domain bingo)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        pos    - (number 0 3)
        num    - (number 0 9)
        numset - (set num)
        board  - (array 4 num)
    )

    (:functions
        (tiles)  - board     ; tile values at each board position
        (called) - numset    ; drawn numbers
        (score)  - pos       ; count of matching positions found (0-3)
    )

    ;; Mark position ?p if its tile value ?n was drawn.
    ;; ?n is bound by (= (read (tiles) ?p) ?n); membership oracle is (called).
    (:action mark
        :parameters (?p - pos ?n - num)
        :precondition (and
            (= (read (tiles) ?p) ?n)
            (member ?n (called))
            (< (score) 3)
        )
        :effect (assign (score) (+ (score) 1))
    )
)
