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
        (last_move) - direction
    )

    (:action move_up
        :parameters (?i ?j - size)
        :precondition (and
            (> ?i 0)
            (= (read (puzzle) (- ?i 1) ?j) 0)
            (not (= (read (puzzle) ?i ?j) 0))
            (not (= (last_move) 2))
        )
        :effect (and
            (write ((puzzle) (- ?i 1) ?j) (read (puzzle) ?i ?j))
            (write ((puzzle) ?i ?j) 0)
            (assign (last_move) 1)
        )
    )

    (:action move_down
        :parameters (?i ?j - size)
        :precondition (and
            (< ?i 3)
            (= (read (puzzle) (+ ?i 1) ?j) 0)
            (not (= (read (puzzle) ?i ?j) 0))
            (not (= (last_move) 1))
        )
        :effect (and
            (write ((puzzle) (+ ?i 1) ?j) (read (puzzle) ?i ?j))
            (write ((puzzle) ?i ?j) 0)
            (assign (last_move) 2)
        )
    )

    (:action move_left
        :parameters (?i ?j - size)
        :precondition (and
            (> ?j 0)
            (= (read (puzzle) ?i (- ?j 1)) 0)
            (not (= (read (puzzle) ?i ?j) 0))
            (not (= (last_move) 4))
        )
        :effect (and
            (write ((puzzle) ?i (- ?j 1)) (read (puzzle) ?i ?j))
            (write ((puzzle) ?i ?j) 0)
            (assign (last_move) 3)
        )
    )

    (:action move_right
        :parameters (?i ?j - size)
        :precondition (and
            (< ?j 3)
            (= (read (puzzle) ?i (+ ?j 1)) 0)
            (not (= (read (puzzle) ?i ?j) 0))
            (not (= (last_move) 3))
        )
        :effect (and
            (write ((puzzle) ?i (+ ?j 1)) (read (puzzle) ?i ?j))
            (write ((puzzle) ?i ?j) 0)
            (assign (last_move) 4)
        )
    )
)
