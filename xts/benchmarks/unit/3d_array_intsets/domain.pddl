;; Test: 3D ARRAY OF SET TYPE — (array 2 2 2 codeset) where codeset = (set code).
;;
;; KEY PATTERN: extending 1D and 2D array-of-sets tests to three dimensions.
;;   Read a 3D cell whose element is a set; test membership; copy between cells.
;;
;; Domain: 2×2×2 grid of code-sets.  propagate copies layer-0 codes into layer-1.
;; Initial: cube[0][0][0]={1,2}, cube[0][0][1]={3}, others={}
;; Goal: member(2, cube[1][0][0])
;; Plan: propagate(0,0)  — 1 step.

(define (domain code-cube)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        layer   - (number 0 1)
        row     - (number 0 1)
        col     - (number 0 1)
        code    - (number 0 4)
        codeset - (set code)
        cube    - (array 2 2 2 codeset)
    )

    (:functions
        (cube) - cube
    )

    ;; Copy layer-0 codes into layer-1 for position (r, c).
    (:action propagate
        :parameters (?r - row ?c - col)
        :precondition ()
        :effect (write ((cube) 1 ?r ?c) (read (cube) 0 ?r ?c))
    )
)
