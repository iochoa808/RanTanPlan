;; PDDL-XTS translation of pddl/test/sokoban-sequential-optimal-strips.
;; FEATURE: 2D arrays + bounded integers.
;;   The location objects pos-R-C and the MOVE-DIR adjacency relation are a
;;   3x3 coordinate grid in disguise.  We index it directly:
;;     (stones) : (array 3 3 (number 0 1))   ; 1 = a stone occupies cell (r,c)
;;     (prow)(pcol) : (number 0 2)           ; the player's coordinates
;;   The four directions become +/-1 on a coordinate (from the MOVE-DIR data:
;;   right/left = row +/-1, down/up = col +/-1).  Boundary handling is implicit:
;;   index expressions like (+ ?pr 2) are only in range for some parameter
;;   values, and IPAR prunes the out-of-range groundings.
;;   The action-cost metric is dropped (irrelevant to satisficing).

(define (domain sokoban-xts)
    (:requirements :arrays :bounded-integers)
    (:types
        size   - (number 0 2)
        cell   - (number 0 1)
        grid_t - (array 3 3 cell)
    )
    (:functions
        (stones) - grid_t
        (prow)   - size
        (pcol)   - size
    )

    ;; ---- player steps into an empty neighbour ----
    (:action move-right
        :parameters (?pr ?pc - size)
        :precondition (and (= (prow) ?pr) (= (pcol) ?pc)
                           (= (read (stones) (+ ?pr 1) ?pc) 0))
        :effect (assign (prow) (+ ?pr 1))
    )
    (:action move-left
        :parameters (?pr ?pc - size)
        :precondition (and (= (prow) ?pr) (= (pcol) ?pc)
                           (= (read (stones) (- ?pr 1) ?pc) 0))
        :effect (assign (prow) (- ?pr 1))
    )
    (:action move-down
        :parameters (?pr ?pc - size)
        :precondition (and (= (prow) ?pr) (= (pcol) ?pc)
                           (= (read (stones) ?pr (+ ?pc 1)) 0))
        :effect (assign (pcol) (+ ?pc 1))
    )
    (:action move-up
        :parameters (?pr ?pc - size)
        :precondition (and (= (prow) ?pr) (= (pcol) ?pc)
                           (= (read (stones) ?pr (- ?pc 1)) 0))
        :effect (assign (pcol) (- ?pc 1))
    )

    ;; ---- player pushes the adjacent stone one cell further ----
    (:action push-right
        :parameters (?pr ?pc - size)
        :precondition (and (= (prow) ?pr) (= (pcol) ?pc)
                           (= (read (stones) (+ ?pr 1) ?pc) 1)
                           (= (read (stones) (+ ?pr 2) ?pc) 0))
        :effect (and (write ((stones) (+ ?pr 1) ?pc) 0)
                     (write ((stones) (+ ?pr 2) ?pc) 1)
                     (assign (prow) (+ ?pr 1)))
    )
    (:action push-left
        :parameters (?pr ?pc - size)
        :precondition (and (= (prow) ?pr) (= (pcol) ?pc)
                           (= (read (stones) (- ?pr 1) ?pc) 1)
                           (= (read (stones) (- ?pr 2) ?pc) 0))
        :effect (and (write ((stones) (- ?pr 1) ?pc) 0)
                     (write ((stones) (- ?pr 2) ?pc) 1)
                     (assign (prow) (- ?pr 1)))
    )
    (:action push-down
        :parameters (?pr ?pc - size)
        :precondition (and (= (prow) ?pr) (= (pcol) ?pc)
                           (= (read (stones) ?pr (+ ?pc 1)) 1)
                           (= (read (stones) ?pr (+ ?pc 2)) 0))
        :effect (and (write ((stones) ?pr (+ ?pc 1)) 0)
                     (write ((stones) ?pr (+ ?pc 2)) 1)
                     (assign (pcol) (+ ?pc 1)))
    )
    (:action push-up
        :parameters (?pr ?pc - size)
        :precondition (and (= (prow) ?pr) (= (pcol) ?pc)
                           (= (read (stones) ?pr (- ?pc 1)) 1)
                           (= (read (stones) ?pr (- ?pc 2)) 0))
        :effect (and (write ((stones) ?pr (- ?pc 1)) 0)
                     (write ((stones) ?pr (- ?pc 2)) 1)
                     (assign (pcol) (- ?pc 1)))
    )
)
