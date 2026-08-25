;; PDDL-XTS full re-modelling of pddl/test/tetris-sequential-optimal's domain
;; (all three piece types: one_square, two_straight, right_l), NOT the
;; single-square minimal-instance shortcut used in
;; PDDL-XTS/translations/tetris-sequential-optimal/domain.pddl.
;;
;; Board is a fixed 4x4 grid (rows/cols 0..3); each right_l element has a
;; FIXED orientation in {0,1,2,3} assigned once in :init and never written by
;; any action.

(define (domain tetris-xts-full)
    (:requirements :arrays :bounded-integers)
    (:types
        piece                          - object
        one_square two_straight right_l - piece
        size   - (number 0 3)
        cell   - (number 0 1)
        board_t - (array 4 4 cell)
        orient - (number 0 3)
    )
    (:functions
        (board)              - board_t
        (srow  ?e - one_square)   - size
        (scol  ?e - one_square)   - size
        (drow1 ?e - two_straight) - size
        (dcol1 ?e - two_straight) - size
        (drow2 ?e - two_straight) - size
        (dcol2 ?e - two_straight) - size
        (lrow  ?e - right_l) - size
        (lcol  ?e - right_l) - size
        (lor   ?e - right_l) - orient
    )

    (:action slide-square-up
        :parameters (?e - one_square ?r ?c - size)
        :precondition (and
            (= (srow ?e) ?r)
            (= (scol ?e) ?c)
            (>= ?r 1)
            (= (read (board) (- ?r 1) ?c) 0)
        )
        :effect (and
            (write ((board) ?r ?c) 0)
            (write ((board) (- ?r 1) ?c) 1)
            (assign (srow ?e) (- ?r 1))
            (assign (scol ?e) ?c)
        )
    )
    (:action slide-square-down
        :parameters (?e - one_square ?r ?c - size)
        :precondition (and (= (srow ?e) ?r) (= (scol ?e) ?c) (<= ?r 2) (= (read (board) (+ ?r 1) ?c) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) (+ ?r 1) ?c) 1) (assign (srow ?e) (+ ?r 1)) (assign (scol ?e) ?c))
    )
    (:action slide-square-left
        :parameters (?e - one_square ?r ?c - size)
        :precondition (and (= (srow ?e) ?r) (= (scol ?e) ?c) (>= ?c 1) (= (read (board) ?r (- ?c 1)) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) ?r (- ?c 1)) 1) (assign (srow ?e) ?r) (assign (scol ?e) (- ?c 1)))
    )
    (:action slide-square-right
        :parameters (?e - one_square ?r ?c - size)
        :precondition (and (= (srow ?e) ?r) (= (scol ?e) ?c) (<= ?c 2) (= (read (board) ?r (+ ?c 1)) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) ?r (+ ?c 1)) 1) (assign (srow ?e) ?r) (assign (scol ?e) (+ ?c 1)))
    )

    (:action slide-two-up
        :parameters (?e - two_straight ?r1 ?c1 ?r2 ?c2 - size)
        :precondition (and (= (drow1 ?e) ?r1) (= (dcol1 ?e) ?c1) (= (drow2 ?e) ?r2) (= (dcol2 ?e) ?c2) (>= ?r2 1) (not (and (= ?r1 (- ?r2 1)) (= ?c1 ?c2))) (= (read (board) (- ?r2 1) ?c2) 0))
        :effect (and (write ((board) ?r1 ?c1) 0) (write ((board) (- ?r2 1) ?c2) 1) (assign (drow1 ?e) ?r2) (assign (dcol1 ?e) ?c2) (assign (drow2 ?e) (- ?r2 1)) (assign (dcol2 ?e) ?c2))
    )
    (:action slide-two-down
        :parameters (?e - two_straight ?r1 ?c1 ?r2 ?c2 - size)
        :precondition (and (= (drow1 ?e) ?r1) (= (dcol1 ?e) ?c1) (= (drow2 ?e) ?r2) (= (dcol2 ?e) ?c2) (<= ?r2 2) (not (and (= ?r1 (+ ?r2 1)) (= ?c1 ?c2))) (= (read (board) (+ ?r2 1) ?c2) 0))
        :effect (and (write ((board) ?r1 ?c1) 0) (write ((board) (+ ?r2 1) ?c2) 1) (assign (drow1 ?e) ?r2) (assign (dcol1 ?e) ?c2) (assign (drow2 ?e) (+ ?r2 1)) (assign (dcol2 ?e) ?c2))
    )
    (:action slide-two-left
        :parameters (?e - two_straight ?r1 ?c1 ?r2 ?c2 - size)
        :precondition (and (= (drow1 ?e) ?r1) (= (dcol1 ?e) ?c1) (= (drow2 ?e) ?r2) (= (dcol2 ?e) ?c2) (>= ?c2 1) (not (and (= ?r1 ?r2) (= ?c1 (- ?c2 1)))) (= (read (board) ?r2 (- ?c2 1)) 0))
        :effect (and (write ((board) ?r1 ?c1) 0) (write ((board) ?r2 (- ?c2 1)) 1) (assign (drow1 ?e) ?r2) (assign (dcol1 ?e) ?c2) (assign (drow2 ?e) ?r2) (assign (dcol2 ?e) (- ?c2 1)))
    )
    (:action slide-two-right
        :parameters (?e - two_straight ?r1 ?c1 ?r2 ?c2 - size)
        :precondition (and (= (drow1 ?e) ?r1) (= (dcol1 ?e) ?c1) (= (drow2 ?e) ?r2) (= (dcol2 ?e) ?c2) (<= ?c2 2) (not (and (= ?r1 ?r2) (= ?c1 (+ ?c2 1)))) (= (read (board) ?r2 (+ ?c2 1)) 0))
        :effect (and (write ((board) ?r1 ?c1) 0) (write ((board) ?r2 (+ ?c2 1)) 1) (assign (drow1 ?e) ?r2) (assign (dcol1 ?e) ?c2) (assign (drow2 ?e) ?r2) (assign (dcol2 ?e) (+ ?c2 1)))
    )

    (:action slide-l-o0-up
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 0) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (>= ?r 2) (>= ?r 1) (<= ?c 2) (= (read (board) (- ?r 2) ?c) 0) (= (read (board) (- ?r 1) (+ ?c 1)) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) ?r (+ ?c 1)) 0) (write ((board) (- ?r 2) ?c) 1) (write ((board) (- ?r 1) (+ ?c 1)) 1) (assign (lrow ?e) (- ?r 1)) (assign (lcol ?e) ?c))
    )
    (:action slide-l-o0-down
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 0) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (<= ?r 2) (<= ?c 2) (= (read (board) (+ ?r 1) ?c) 0) (= (read (board) (+ ?r 1) (+ ?c 1)) 0))
        :effect (and (write ((board) (- ?r 1) ?c) 0) (write ((board) ?r (+ ?c 1)) 0) (write ((board) (+ ?r 1) ?c) 1) (write ((board) (+ ?r 1) (+ ?c 1)) 1) (assign (lrow ?e) (+ ?r 1)) (assign (lcol ?e) ?c))
    )
    (:action slide-l-o0-left
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 0) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (>= ?c 1) (>= ?r 1) (= (read (board) ?r (- ?c 1)) 0) (= (read (board) (- ?r 1) (- ?c 1)) 0))
        :effect (and (write ((board) (- ?r 1) ?c) 0) (write ((board) ?r (+ ?c 1)) 0) (write ((board) ?r (- ?c 1)) 1) (write ((board) (- ?r 1) (- ?c 1)) 1) (assign (lrow ?e) ?r) (assign (lcol ?e) (- ?c 1)))
    )
    (:action slide-l-o0-right
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 0) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (>= ?r 1) (<= ?c 2) (<= ?c 1) (= (read (board) (- ?r 1) (+ ?c 1)) 0) (= (read (board) ?r (+ ?c 2)) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) (- ?r 1) ?c) 0) (write ((board) (- ?r 1) (+ ?c 1)) 1) (write ((board) ?r (+ ?c 2)) 1) (assign (lrow ?e) ?r) (assign (lcol ?e) (+ ?c 1)))
    )
    (:action slide-l-o1-up
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 1) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (>= ?r 1) (<= ?c 2) (= (read (board) (- ?r 1) ?c) 0) (= (read (board) (- ?r 1) (+ ?c 1)) 0))
        :effect (and (write ((board) ?r (+ ?c 1)) 0) (write ((board) (+ ?r 1) ?c) 0) (write ((board) (- ?r 1) ?c) 1) (write ((board) (- ?r 1) (+ ?c 1)) 1) (assign (lrow ?e) (- ?r 1)) (assign (lcol ?e) ?c))
    )
    (:action slide-l-o1-down
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 1) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (<= ?r 2) (<= ?c 2) (<= ?r 1) (= (read (board) (+ ?r 1) (+ ?c 1)) 0) (= (read (board) (+ ?r 2) ?c) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) ?r (+ ?c 1)) 0) (write ((board) (+ ?r 1) (+ ?c 1)) 1) (write ((board) (+ ?r 2) ?c) 1) (assign (lrow ?e) (+ ?r 1)) (assign (lcol ?e) ?c))
    )
    (:action slide-l-o1-left
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 1) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (>= ?c 1) (<= ?r 2) (= (read (board) ?r (- ?c 1)) 0) (= (read (board) (+ ?r 1) (- ?c 1)) 0))
        :effect (and (write ((board) ?r (+ ?c 1)) 0) (write ((board) (+ ?r 1) ?c) 0) (write ((board) ?r (- ?c 1)) 1) (write ((board) (+ ?r 1) (- ?c 1)) 1) (assign (lrow ?e) ?r) (assign (lcol ?e) (- ?c 1)))
    )
    (:action slide-l-o1-right
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 1) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (<= ?c 1) (<= ?r 2) (<= ?c 2) (= (read (board) ?r (+ ?c 2)) 0) (= (read (board) (+ ?r 1) (+ ?c 1)) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) (+ ?r 1) ?c) 0) (write ((board) ?r (+ ?c 2)) 1) (write ((board) (+ ?r 1) (+ ?c 1)) 1) (assign (lrow ?e) ?r) (assign (lcol ?e) (+ ?c 1)))
    )
    (:action slide-l-o2-up
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 2) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (>= ?r 1) (>= ?c 1) (= (read (board) (- ?r 1) ?c) 0) (= (read (board) (- ?r 1) (- ?c 1)) 0))
        :effect (and (write ((board) (+ ?r 1) ?c) 0) (write ((board) ?r (- ?c 1)) 0) (write ((board) (- ?r 1) ?c) 1) (write ((board) (- ?r 1) (- ?c 1)) 1) (assign (lrow ?e) (- ?r 1)) (assign (lcol ?e) ?c))
    )
    (:action slide-l-o2-down
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 2) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (<= ?r 1) (<= ?r 2) (>= ?c 1) (= (read (board) (+ ?r 2) ?c) 0) (= (read (board) (+ ?r 1) (- ?c 1)) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) ?r (- ?c 1)) 0) (write ((board) (+ ?r 2) ?c) 1) (write ((board) (+ ?r 1) (- ?c 1)) 1) (assign (lrow ?e) (+ ?r 1)) (assign (lcol ?e) ?c))
    )
    (:action slide-l-o2-left
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 2) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (<= ?r 2) (>= ?c 1) (>= ?c 2) (= (read (board) (+ ?r 1) (- ?c 1)) 0) (= (read (board) ?r (- ?c 2)) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) (+ ?r 1) ?c) 0) (write ((board) (+ ?r 1) (- ?c 1)) 1) (write ((board) ?r (- ?c 2)) 1) (assign (lrow ?e) ?r) (assign (lcol ?e) (- ?c 1)))
    )
    (:action slide-l-o2-right
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 2) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (<= ?c 2) (<= ?r 2) (= (read (board) ?r (+ ?c 1)) 0) (= (read (board) (+ ?r 1) (+ ?c 1)) 0))
        :effect (and (write ((board) (+ ?r 1) ?c) 0) (write ((board) ?r (- ?c 1)) 0) (write ((board) ?r (+ ?c 1)) 1) (write ((board) (+ ?r 1) (+ ?c 1)) 1) (assign (lrow ?e) ?r) (assign (lcol ?e) (+ ?c 1)))
    )
    (:action slide-l-o3-up
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 3) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (>= ?r 1) (>= ?c 1) (>= ?r 2) (= (read (board) (- ?r 1) (- ?c 1)) 0) (= (read (board) (- ?r 2) ?c) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) ?r (- ?c 1)) 0) (write ((board) (- ?r 1) (- ?c 1)) 1) (write ((board) (- ?r 2) ?c) 1) (assign (lrow ?e) (- ?r 1)) (assign (lcol ?e) ?c))
    )
    (:action slide-l-o3-down
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 3) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (<= ?r 2) (>= ?c 1) (= (read (board) (+ ?r 1) ?c) 0) (= (read (board) (+ ?r 1) (- ?c 1)) 0))
        :effect (and (write ((board) ?r (- ?c 1)) 0) (write ((board) (- ?r 1) ?c) 0) (write ((board) (+ ?r 1) ?c) 1) (write ((board) (+ ?r 1) (- ?c 1)) 1) (assign (lrow ?e) (+ ?r 1)) (assign (lcol ?e) ?c))
    )
    (:action slide-l-o3-left
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 3) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (>= ?c 2) (>= ?r 1) (>= ?c 1) (= (read (board) ?r (- ?c 2)) 0) (= (read (board) (- ?r 1) (- ?c 1)) 0))
        :effect (and (write ((board) ?r ?c) 0) (write ((board) (- ?r 1) ?c) 0) (write ((board) ?r (- ?c 2)) 1) (write ((board) (- ?r 1) (- ?c 1)) 1) (assign (lrow ?e) ?r) (assign (lcol ?e) (- ?c 1)))
    )
    (:action slide-l-o3-right
        :parameters (?e - right_l ?r ?c - size)
        :precondition (and (= (lor ?e) 3) (= (lrow ?e) ?r) (= (lcol ?e) ?c) (<= ?c 2) (>= ?r 1) (= (read (board) ?r (+ ?c 1)) 0) (= (read (board) (- ?r 1) (+ ?c 1)) 0))
        :effect (and (write ((board) ?r (- ?c 1)) 0) (write ((board) (- ?r 1) ?c) 0) (write ((board) ?r (+ ?c 1)) 1) (write ((board) (- ?r 1) (+ ?c 1)) 1) (assign (lrow ?e) ?r) (assign (lcol ?e) (+ ?c 1)))
    )

)
