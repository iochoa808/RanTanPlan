;; Test: static 2D adjacency matrix used only in preconditions.
;;
;; Covers: StaticFluentPass for a 2D array that appears exclusively in
;; preconditions (never in effects). After substitution, each grounded
;; action's precondition becomes a literal integer comparison with no
;; remaining array variable.
;;
;; adj (3×3, static, 0=blocked 1=allowed):
;;   row 0: [0, 1, 0]
;;   row 1: [0, 0, 1]
;;   row 2: [1, 0, 0]

(define (domain static-adj)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        node    - (number 0 2)
        bit     - (number 0 1)
        adj_t   - (array 3 3 bit)
    )

    (:functions
        (adj) - adj_t   ;; static — never written by any action
        (pos) - node
    )

    ;; Move from ?from to ?to if the adjacency table allows it.
    (:action move
        :parameters (?from - node ?to - node)
        :precondition (and
            (= (pos) ?from)
            (= (read (adj) ?from ?to) 1)
        )
        :effect (assign (pos) ?to)
    )
)
