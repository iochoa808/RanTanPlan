(define (domain fifteen-puzzle)

    (:requirements :arrays)

    (:types
        size      - (number 0 3)
        range     - (number 0 15)
        puzzle15  - (array 4 4 range)
        direction - (number 0 4)   ; 0=none 1=up 2=down 3=left 4=right
    )

    (:functions
        (puzzle)    - puzzle15
        (blank_row) - size
        (blank_col) - size
        (last_move) - direction
    )

    ; blank_row/blank_col track the blank's current position.
    ; Preconditions use these instead of array reads, which is cheaper and
    ; implicitly handles boundary conditions (blank_row = i-1 is unsatisfiable
    ; when i=0 because blank_row >= 0, so no explicit >= guard is needed).

    (:action move_up
        :parameters (?i ?j - size)
        :precondition (and
            (= (blank_row) (- ?i 1))
            (= (blank_col) ?j)
            (not (= (last_move) 2))
        )
        :effect (and
            (write ((puzzle) (- ?i 1) ?j) (read (puzzle) ?i ?j))
            (write ((puzzle) ?i ?j) 0)
            (assign (blank_row) ?i)
            (assign (blank_col) ?j)
            (assign (last_move) 1)
        )
    )

    (:action move_down
        :parameters (?i ?j - size)
        :precondition (and
            (= (blank_row) (+ ?i 1))
            (= (blank_col) ?j)
            (not (= (last_move) 1))
        )
        :effect (and
            (write ((puzzle) (+ ?i 1) ?j) (read (puzzle) ?i ?j))
            (write ((puzzle) ?i ?j) 0)
            (assign (blank_row) ?i)
            (assign (blank_col) ?j)
            (assign (last_move) 2)
        )
    )

    (:action move_left
        :parameters (?i ?j - size)
        :precondition (and
            (= (blank_row) ?i)
            (= (blank_col) (- ?j 1))
            (not (= (last_move) 4))
        )
        :effect (and
            (write ((puzzle) ?i (- ?j 1)) (read (puzzle) ?i ?j))
            (write ((puzzle) ?i ?j) 0)
            (assign (blank_row) ?i)
            (assign (blank_col) ?j)
            (assign (last_move) 3)
        )
    )

    (:action move_right
        :parameters (?i ?j - size)
        :precondition (and
            (= (blank_row) ?i)
            (= (blank_col) (+ ?j 1))
            (not (= (last_move) 3))
        )
        :effect (and
            (write ((puzzle) ?i (+ ?j 1)) (read (puzzle) ?i ?j))
            (write ((puzzle) ?i ?j) 0)
            (assign (blank_row) ?i)
            (assign (blank_col) ?j)
            (assign (last_move) 4)
        )
    )
)
