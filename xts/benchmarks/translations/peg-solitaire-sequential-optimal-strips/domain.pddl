;; PDDL-XTS translation of pddl/test/peg-solitaire-sequential-optimal-strips.
;; FEATURE: arrays (1D) + bounded integers.
;;   The board (occupied/free per location) becomes a 1D array of 0/1 cells.
;;   The minimal instance is three locations in a row with the single line
;;   (IN-LINE loc1 loc2 loc3), so a jump is the fixed index pattern
;;   i -> i+1 -> i+2: peg at i and i+1, hole at i+2.  Index arithmetic on a
;;   bounded index type encodes the geometry directly; IPAR prunes the
;;   out-of-range groundings (only i=0 survives, since i+2 must be <= 2).
;;   The move-chaining control (move-ended / last-visited) becomes a 0/1
;;   "moving" flag plus a bounded "last_pos" index.
;;   (A general cross-shaped board would keep the IN-LINE triples as data or
;;   use a 2D array; the in-a-row instance maps to pure index arithmetic.)

(define (domain pegsolitaire-xts)
    (:requirements :arrays :bounded-integers)
    (:types
        idx     - (number 0 2)
        occ     - (number 0 1)
        board_t - (array 3 occ)
        flag    - (number 0 1)
    )
    (:functions
        (pegs)     - board_t
        (moving)   - flag
        (last_pos) - idx
    )

    ;; Start a move: jump from i over i+1 into the hole at i+2.
    (:action jump-new-move
        :parameters (?i - idx)
        :precondition (and
            (= (moving) 0)
            (= (read (pegs) ?i) 1)
            (= (read (pegs) (+ ?i 1)) 1)
            (= (read (pegs) (+ ?i 2)) 0)
        )
        :effect (and
            (write (pegs) (?i) 0)
            (write (pegs) ((+ ?i 1)) 0)
            (write (pegs) ((+ ?i 2)) 1)
            (assign (moving) 1)
            (assign (last_pos) (+ ?i 2))
        )
    )

    ;; Continue the chain from the last landing cell.
    (:action jump-continue-move
        :parameters (?i - idx)
        :precondition (and
            (= (moving) 1)
            (= (last_pos) ?i)
            (= (read (pegs) ?i) 1)
            (= (read (pegs) (+ ?i 1)) 1)
            (= (read (pegs) (+ ?i 2)) 0)
        )
        :effect (and
            (write (pegs) (?i) 0)
            (write (pegs) ((+ ?i 1)) 0)
            (write (pegs) ((+ ?i 2)) 1)
            (assign (last_pos) (+ ?i 2))
        )
    )

    (:action end-move
        :parameters ()
        :precondition (= (moving) 1)
        :effect (assign (moving) 0)
    )
)
