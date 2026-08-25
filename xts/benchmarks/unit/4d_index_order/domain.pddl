;; Test: 4D array (4×3×2×1) — index ordering.
;;
;; Each dimension has a DIFFERENT size (4, 3, 2, 1).  The four goal cells are
;; chosen so that each one exercises a different dimension at its maximum index:
;;
;;   arr[3][0][0][0] = 1   → max d0 (3), all others 0
;;   arr[0][2][0][0] = 2   → max d1 (2), all others 0
;;   arr[0][0][1][0] = 3   → max d2 (1), all others 0
;;   arr[3][2][1][0] = 4   → all dimensions at their maximum
;;
;; If any two dimension indices are swapped in the encoder the corresponding
;; "set" action would need to supply an out-of-range index for that type and
;; the problem becomes UNSOLVABLE (the goal cell is never written).

(define (domain test-4d-order)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        d0  - (number 0 3)   ;; size 4 — indices 0..3
        d1  - (number 0 2)   ;; size 3 — indices 0..2
        d2  - (number 0 1)   ;; size 2 — indices 0..1
        d3  - (number 0 0)   ;; size 1 — index  0
        val - (number 0 3)
        arr_t - (array 4 3 2 1 val)
    )

    (:functions
        (arr) - arr_t
    )

    (:action set
        :parameters (?a - d0 ?b - d1 ?c - d2 ?e - d3 ?v - val)
        :precondition (and
            (> ?v 0)
            (= (read (arr) ?a ?b ?c ?e) 0)
        )
        :effect (write ((arr) ?a ?b ?c ?e) ?v)
    )

    (:action clear
        :parameters (?a - d0 ?b - d1 ?c - d2 ?e - d3)
        :precondition (> (read (arr) ?a ?b ?c ?e) 0)
        :effect (write ((arr) ?a ?b ?c ?e) 0)
    )
)
