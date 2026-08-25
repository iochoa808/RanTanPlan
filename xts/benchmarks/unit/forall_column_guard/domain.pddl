;; Test: FORALL OVER AN ARRAY COLUMN, NEXT TO AN ACTION SHARING THE PARAMETER NAME.
;;
;; KEY PATTERN: `rotate_col_up` guards column ?c with
;;   (forall (?k - idx) (not (= (marker) (read (board) ?k ?c))))
;; while the EARLIER action `scan` declares a parameter of the same name and type.
;; IPAR grounds `scan` first; expanding the forall then appends ?k to the
;; instantiation tuple. Both actions share one interned ?c node, so a substitution
;; cache keyed only on that tuple lets scan's substitution answer for ?c — every
;; rotate_col_up_k ends up guarding the DIAGONAL board[j][j] instead of column k.
;;
;; Domain: 3x3 integer grid plus a marker value. Rotating a column upwards is
;; allowed only while the marker sits nowhere in that column.
;;
;; Initial: board=[[0,1,2],[3,4,5],[6,7,8]], marker=4 — which sits on board[1][1].
;;   Column 0 is [0,3,6] and holds no 4, so rotate_col_up(0) is allowed and lifts
;;   board[1][0]=3 into board[0][0].
;;   Under the diagonal mis-expansion the guard reads board[1][1]=4 instead, so
;;   EVERY column rotation is blocked and the goal becomes unreachable.
;;
;; Goal: board[0][0]=3.  Plan: rotate_col_up(0) — 1 step.

(define (domain forall-column-guard)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx   - (number 0 2)
        val   - (number 0 8)
        count - (number 0 3)
        grid  - (array 3 3 val)
    )

    (:functions
        (board)  - grid
        (marker) - val
        (probes) - count
    )

    ;; Declared first so its groundings reach the substitution cache first.
    ;; Reads ?r and ?c; never touches (board), so it offers no route to the goal.
    (:action scan
        :parameters (?r - idx ?c - idx)
        :precondition (= (read (board) ?r ?c) (marker))
        :effect (increase (probes) 1)
    )

    ;; Rotate column ?c up by one, but only while the marker is out of that column.
    (:action rotate_col_up
        :parameters (?c - idx)
        :precondition (forall (?k - idx) (not (= (marker) (read (board) ?k ?c))))
        :effect (and
            (write ((board) 0 ?c) (read (board) 1 ?c))
            (write ((board) 1 ?c) (read (board) 2 ?c))
            (write ((board) 2 ?c) (read (board) 0 ?c))
        )
    )
)
