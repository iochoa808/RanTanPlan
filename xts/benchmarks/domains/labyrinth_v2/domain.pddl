(define (domain labyrinth2)
    (:requirements :typing :adl :arrays :bounded-integers :sets)

    (:types
        direction - object
        idx       - (number 0 3)
        dir_set   - (set direction)
        grid      - (array 4 4 dir_set)
    )

    (:constants N S E W - direction)

    (:functions
        (card_at)   - grid
        (robot-row) - idx
        (robot-col) - idx
    )

    ; ---------------------------------------------------------------
    ; Movement
    ; ---------------------------------------------------------------

    (:action move_north
        :parameters (?r ?c - idx)
        :precondition (and
            (>= ?r 1)
            (= (robot-row) ?r)
            (= (robot-col) ?c)
            (member N (read (card_at) ?r ?c))
            (member S (read (card_at) (- ?r 1) ?c))
        )
        :effect (decrease (robot-row) 1)
    )

    (:action move_south
        :parameters (?r ?c - idx)
        :precondition (and
            (<= ?r 2)
            (= (robot-row) ?r)
            (= (robot-col) ?c)
            (member S (read (card_at) ?r ?c))
            (member N (read (card_at) (+ ?r 1) ?c))
        )
        :effect (increase (robot-row) 1)
    )

    (:action move_east
        :parameters (?r ?c - idx)
        :precondition (and
            (<= ?c 2)
            (= (robot-row) ?r)
            (= (robot-col) ?c)
            (member E (read (card_at) ?r ?c))
            (member W (read (card_at) ?r (+ ?c 1)))
        )
        :effect (increase (robot-col) 1)
    )

    (:action move_west
        :parameters (?r ?c - idx)
        :precondition (and
            (>= ?c 1)
            (= (robot-row) ?r)
            (= (robot-col) ?c)
            (member W (read (card_at) ?r ?c))
            (member E (read (card_at) ?r (- ?c 1)))
        )
        :effect (decrease (robot-col) 1)
    )

    ; ---------------------------------------------------------------
    ; Rotation — direction sets live in the grid cells, so rotating
    ; a row/col automatically carries each cell's open-dirs with it.
    ; Precondition: one inequality check suffices (robot not in the
    ; moving row/col), replacing 4 explicit card-identity checks.
    ; ---------------------------------------------------------------

    ;; The rotations are written as a forall over the shifted range plus the
    ;; single wrap-around write. The forall range stops one short of the array
    ;; bound so that (+ ?i 1) / (- ?i 1) stays in [0,3] without a guard; the
    ;; cell that wraps is the one the loop leaves out.

    (:action rotate_col_up
        :parameters (?c - idx)
        :precondition (not (= (robot-col) ?c))
        :effect (and
            (forall (?i - (number 0 2))
                (write ((card_at) ?i ?c) (read (card_at) (+ ?i 1) ?c)))
            (write ((card_at) 3 ?c) (read (card_at) 0 ?c))
        )
    )

    (:action rotate_col_down
        :parameters (?c - idx)
        :precondition (not (= (robot-col) ?c))
        :effect (and
            (forall (?i - (number 1 3))
                (write ((card_at) ?i ?c) (read (card_at) (- ?i 1) ?c)))
            (write ((card_at) 0 ?c) (read (card_at) 3 ?c))
        )
    )

    (:action rotate_row_left
        :parameters (?r - idx)
        :precondition (not (= (robot-row) ?r))
        :effect (and
            (forall (?j - (number 0 2))
                (write ((card_at) ?r ?j) (read (card_at) ?r (+ ?j 1))))
            (write ((card_at) ?r 3) (read (card_at) ?r 0))
        )
    )

    (:action rotate_row_right
        :parameters (?r - idx)
        :precondition (not (= (robot-row) ?r))
        :effect (and
            (forall (?j - (number 1 3))
                (write ((card_at) ?r ?j) (read (card_at) ?r (- ?j 1))))
            (write ((card_at) ?r 0) (read (card_at) ?r 3))
        )
    )
)
