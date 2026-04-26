;; ============================================================
;; Incremental achievers cascade test
;; ============================================================
;;
;; Purpose: exercise the multi-iteration ActionPruningPass loop and
;; the incremental AchieversAnalysis::update() path with non-trivial
;; Z3 re-verification queries.
;;
;; Pattern: a "waste" action contributes to a fluent's RPG bound but
;; is not goal-relevant. A "bridge" action's achievement of a
;; precondition depends on that bound (through a value expression,
;; not a precondition). When waste is removed in iteration 1, the
;; bound tightens, the bridge's achievement query flips SAT->UNSAT
;; in iteration 2, removing the bridge transitively. An alternative
;; "direct-set" path keeps the goal reachable so the RPG does not
;; short-circuit to UNSOLVABLE before the achievers update runs.
;;
;; Expected trace:
;;   Iter 1: full Z3 analysis, BFS removes `waste`
;;   Iter 2: incremental update, ~2 Z3 re-verifications fire,
;;           `(bridge, y>=50)` flips SAT->UNSAT, removes bridge + trigger
;;   Iter 3: fixpoint, 0 Z3 queries, plan found via direct-set
;;
;; This is the only entry in pddl/test/ that triggers the incremental
;; cache update with non-zero Z3 re-verification queries.
;; ============================================================

(define (domain incremental-cascade)
  (:requirements :strips :numeric-fluents)
  (:predicates (g))
  (:functions (x) (y))

  ;; Goal-irrelevant; only effect is on x. Removed in iter 1.
  ;; Its presence inflates x's RPG bound to [0,100].
  (:action waste
    :parameters ()
    :effect (assign (x) 100))

  ;; Boolean-precondition path. Effect "y := x" makes its achievement of
  ;; (>= (y) 50) depend on x's bound. After waste is removed, x collapses
  ;; to [0,0] and bridge's achievement flips SAT->UNSAT in iter 2.
  (:action bridge
    :parameters ()
    :precondition (g)
    :effect (assign (y) (x)))

  ;; Achiever of bridge's precondition. Becomes goal-irrelevant once
  ;; bridge is pruned in iter 2.
  (:action trigger
    :parameters ()
    :effect (g))

  ;; Direct path. Always achieves (>= (y) 50) via constant assign.
  ;; Keeps the goal RPG-reachable through every iteration so the
  ;; achievers update path actually runs (RPG does not short-circuit).
  (:action direct-set
    :parameters ()
    :effect (assign (y) 50))
)
