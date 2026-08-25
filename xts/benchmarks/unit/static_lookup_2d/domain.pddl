;; Test: 2D static array used as a lookup table in preconditions and effects.
;;
;; No action ever writes to (dist_table), so StaticFluentPass marks it as static
;; and substitutes every occurrence with its initial ARRAY_CONSTANT value.
;; After substitution, (read (dist_table) ?i ?j) in grounded actions becomes a
;; concrete integer that the solver sees as a literal — no array variable at all.
;;
;; This exercises:
;;   1. StaticFluentPass for 2D arrays (if→while N-D fix).
;;   2. ARRAY_READ(ARRAY_CONSTANT(...), i, j) evaluation in preconditions.
;;   3. ARRAY_READ(ARRAY_CONSTANT(...), i, j) as an effect value expression.
;;
;; dist_table (3×3):
;;   row 0: [1, 3, 8]
;;   row 1: [2, 7, 4]
;;   row 2: [9, 5, 6]
;;
;; Only cells with value > 5: (0,2)=8, (1,1)=7, (2,0)=9, (2,2)=6.

(define (domain static-lookup)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        node   - (number 0 2)
        dist   - (number 0 9)
        table_t - (array 3 3 dist)
    )

    (:functions
        (dist_table) - table_t   ;; static — never written by any action
        (cost)       - dist
    )

    ;; Record the distance between two nodes if it exceeds 5.
    (:action record
        :parameters (?i - node ?j - node)
        :precondition (and
            (= (cost) 0)
            (> (read (dist_table) ?i ?j) 5)
        )
        :effect (assign (cost) (read (dist_table) ?i ?j))
    )
)
