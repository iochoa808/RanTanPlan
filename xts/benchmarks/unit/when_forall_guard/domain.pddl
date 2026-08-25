;; Test: WHEN guarded by a FORALL CONDITION.
;;
;; KEY PATTERN: (when (forall (?i - (number 0 2)) (= (read (arr) ?i) 0)) (done))
;;   The when-guard itself is a universal quantifier over the array.
;;   Existing when tests use predicates or arithmetic comparisons as guards.
;;   This exercises a composite quantified guard.
;;
;; Domain: 3-cell array; check() fires (done) only when ALL cells are 0.
;; Also includes reset(?i) to write 0 into a cell so the guard can be met.
;;
;; Initial: arr=[0,5,0]; goal: done.
;; Plan: reset(1), check()  — 2 steps.

(define (domain forall-when-guard)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx - (number 0 2)
        val - (number 0 9)
        arr - (array 3 val)
    )

    (:predicates
        (done)
    )

    (:functions
        (cells) - arr
    )

    ;; Zero out one cell.
    (:action reset
        :parameters (?i - idx)
        :precondition (> (read (cells) ?i) 0)
        :effect (write (cells) (?i) 0)
    )

    ;; Check: if ALL cells are 0, mark done.
    ;; The when-guard is a forall quantifier over the array.
    (:action check
        :parameters ()
        :precondition (not (done))
        :effect (when (forall (?i - (number 0 2)) (= (read (cells) ?i) 0))
                    (done))
    )
)
