;; Test: STATIC 3D LOOKUP TABLE — 3D array used only in preconditions, never written.
;;
;; KEY PATTERN: StaticFluentPass for a 3D array that appears exclusively in
;; preconditions. After substitution, each grounded action's precondition
;; becomes a literal bit comparison with no remaining array variable.
;;
;; reachable is a 2×2×2 bit table (depth × row × col).
;; move(?d,?r,?c) is allowed only when reachable[d][r][c] = 1.
;; Initial position: d=0, r=0, c=0.  reachable[0][1][0] = 1.
;; Goal: d=0, r=1, c=0.
;; Plan: move(0,1,0)  — 1 step.

(define (domain static-3d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        d-idx   - (number 0 1)
        r-idx   - (number 0 1)
        c-idx   - (number 0 1)
        bit     - (number 0 1)
        table-t - (array 2 2 2 bit)
    )

    (:functions
        (reachable) - table-t   ;; static — never written by any action
        (d)         - d-idx
        (r)         - r-idx
        (c)         - c-idx
    )

    ;; Move to position (?nd, ?nr, ?nc) if the lookup table allows it.
    (:action move
        :parameters (?nd - d-idx ?nr - r-idx ?nc - c-idx)
        :precondition (and
            (= (read (reachable) ?nd ?nr ?nc) 1)
        )
        :effect (and
            (assign (d) ?nd)
            (assign (r) ?nr)
            (assign (c) ?nc)
        )
    )
)
