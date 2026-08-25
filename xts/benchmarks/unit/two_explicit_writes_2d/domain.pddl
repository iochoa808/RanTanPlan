;; Test: constant-index writes on a 2D array — the multi-bracket IPAR cell path.
;;
;; KEY PATTERN: (write ((board) 0 1) 5) with ALL indices constant.
;; Constant subscripts compile to arity-0 IPAR cell SVs with multi-bracket
;; names ("board[0][1]"), unlike two_explicit_writes which only produces
;; single-bracket 1D names ("cells[0]").  Exercises the shared
;; parse_ipar_cell_name on 2D names in: cell-SV write encoding (store chain
;; into the parent array), the read fallback (nested select), frame axioms
;; via array_epc_index_, and CWA initial values for 2D cells.
;;
;; write_b's precondition reads the cell written by write_a, forcing a
;; 2-step plan across timesteps (frame axioms must carry cell values).

(define (domain two-writes-2d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val  - (number 0 9)
        grid - (array 2 3 val)
    )

    (:functions
        (board) - grid
    )

    (:action write_a
        :parameters ()
        :precondition (= (read (board) (0) (1)) 0)
        :effect (write ((board) 0 1) 5)
    )

    (:action write_b
        :parameters ()
        :precondition (and
            (= (read (board) (0) (1)) 5)
            (= (read (board) (1) (2)) 0)
        )
        :effect (write ((board) 1 2) 7)
    )
)
