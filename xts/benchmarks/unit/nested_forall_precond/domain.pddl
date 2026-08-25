;; Test: NESTED FORALL IN PRECONDITION (VALID).
;;
;; KEY PATTERN: (forall (?i - ...) (forall (?j - ...) body)) in a precondition.
;;   nested_forall_effect tests (and confirms as invalid) nested forall in EFFECTS.
;;   In PRECONDITIONS, nested forall is a valid universal quantifier over two
;;   variables — no restriction applies there.
;;
;; Domain: 2×2 integer grid.  verify() fires only when ALL cells equal 1.
;;   The forall nesting checks every (i,j) pair: forall i: forall j: cell[i][j]=1.
;;
;; Initial: grid=[[1,1],[1,0]]; goal: done.
;; Plan: set(1,1), verify()  — 2 steps.

(define (domain nested-forall-prec)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx  - (number 0 1)
        val  - (number 0 1)
        arr2 - (array 2 2 val)
    )

    (:predicates
        (done)
    )

    (:functions
        (grid) - arr2
    )

    ;; Set cell [?i][?j] to 1.
    (:action set
        :parameters (?i - idx ?j - idx)
        :precondition (= (read (grid) ?i ?j) 0)
        :effect (write ((grid) ?i ?j) 1)
    )

    ;; verify: precondition uses nested forall (valid in preconditions).
    (:action verify
        :parameters ()
        :precondition (and
            (not (done))
            (forall (?i - (number 0 1))
                (forall (?j - (number 0 1))
                    (= (read (grid) ?i ?j) 1)))
        )
        :effect (done)
    )
)
