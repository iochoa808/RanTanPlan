;; Test: 2D array whose element type is a set of BOUNDED INTEGERS.
;;
;; Covers: (array 2 3 codeset) where codeset = (set code) and code is a bounded int.
;; Previously only 1D arrays of int-sets were tested (domain_array_intsets.pddl).
;;
;; Domain: 2×3 access-code grid; propagate copies row-0 codes into row-1.
;; Column parameter ?c is constrained by membership in (active_c) set.

(define (domain code-grid)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        row     - (number 0 1)
        col     - (number 0 2)
        code    - (number 0 4)
        codeset - (set code)
        colset  - (set col)
        grid    - (array 2 3 codeset)
    )

    (:functions
        (codes)    - grid
        (active_c) - colset   ; columns scheduled for propagation
    )

    ;; Copy row-0 codes into row-1 for each active column.
    ;; Row index 0 is a constant so ArrayCompilatorPass can specialise it.
    (:action propagate
        :parameters (?c - col)
        :precondition (member ?c (active_c))
        :effect (write ((codes) 1 ?c) (read (codes) 0 ?c))
    )
)
