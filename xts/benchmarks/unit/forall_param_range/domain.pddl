;; Test: FORALL with PARAMETER as UPPER bound (mark_prefix) AND as BOTH bounds
;;       (check_window) in preconditions with array reads.
;;
;; KEY PATTERNS:
;;   mark_prefix:  (forall (?i - (number 0 ?n))    ...)  — param upper bound only
;;   check_window: (forall (?i - (number ?lo ?hi)) ...)  — param lower AND upper bound
;;
;; Domain: 5-slot array; both prefix and window must be confirmed all-zero.

(define (domain forall-param-array)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx - (number 0 4)
        val - (number 0 9)
        arr - (array 5 val)
    )

    (:predicates
        (prefix_done)
        (window_clear)
    )

    (:functions
        (cells) - arr
    )

    ;; Zero out one cell.
    (:action zero
        :parameters (?i - idx)
        :precondition (> (read (cells) ?i) 0)
        :effect (write (cells) (?i) 0)
    )

    ;; Declare prefix [0..?n] done — fires only if every cell in [0..?n] is 0.
    (:action mark_prefix
        :parameters (?n - idx)
        :precondition (forall (?i - (number 0 ?n)) (= (read (cells) ?i) 0))
        :effect (prefix_done)
    )

    ;; Declare window [?lo..?hi] clear — forall with BOTH bounds as parameters.
    (:action check_window
        :parameters (?lo - idx ?hi - idx)
        :precondition (and
            (<= ?lo ?hi)
            (forall (?i - (number ?lo ?hi)) (= (read (cells) ?i) 0))
        )
        :effect (window_clear)
    )
)
