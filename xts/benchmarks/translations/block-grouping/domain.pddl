;; PDDL-XTS translation of pddl/test/block-grouping.
;; FEATURE: bounded integers.
;;   Each block has grid coordinates (x ?b)/(y ?b); the static grid bounds
;;   (min_x/max_x/min_y/max_y = 1..5) fold into the coordinate type (number 1 5).
;;   The move guards (<= (+ (y ?b) 1) (max_y)) become (< (y ?b) 5), etc.
;;   (These are independent per-block coordinates, not a grid array of contents,
;;   so bounded ints — not arrays — are the right fit.)

(define (domain block-grouping-xts)
    (:requirements :numeric-fluents :typing :bounded-integers)
    (:types
        block - object
        coord - (number 1 5)
    )
    (:functions
        (x ?b - block) - coord
        (y ?b - block) - coord
    )

    (:action move_block_up
        :parameters (?b - block)
        :precondition (< (y ?b) 5)
        :effect (increase (y ?b) 1)
    )
    (:action move_block_down
        :parameters (?b - block)
        :precondition (> (y ?b) 1)
        :effect (decrease (y ?b) 1)
    )
    (:action move_block_right
        :parameters (?b - block)
        :precondition (< (x ?b) 5)
        :effect (increase (x ?b) 1)
    )
    (:action move_block_left
        :parameters (?b - block)
        :precondition (> (x ?b) 1)
        :effect (decrease (x ?b) 1)
    )
)
