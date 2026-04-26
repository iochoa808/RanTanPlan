;; ============================================================
;; Incremental achievers cascade test (3-step)
;; ============================================================
;;
;; Purpose: exercise THREE consecutive iterations of the
;; ActionPruningPass loop, each firing real Z3 re-verification
;; queries on the AchieversAnalysis::update() path.
;;
;; Trick: trigger is a multi-effect action. It contributes to
;; bound on h (via constant assign h:=100) AND achieves the
;; Boolean precondition g for bridge. In iter 2:
;;   - bridge is removed (achievement of y>=50 flips: x has
;;     tightened to [0,0]).
;;   - trigger is BFS-orphaned (its only consumer of g was
;;     bridge; nothing else needs g).
;;   - But trigger is still in the action set DURING iter 2's
;;     RPG, so h is still bounded [0,100], and bridge2's
;;     achievement of z>=50 (depends on h via "z := h") stays SAT.
;; In iter 3, with trigger now actually removed, the RPG sees
;; h: [0,100] -> [0,0]. bridge2's achievement flips, removing
;; bridge2 + trigger2.
;;
;; Expected trace:
;;   Iter 1: full Z3 analysis, BFS removes `waste`
;;   Iter 2: incremental update, ~3 Z3 re-verifications,
;;           removes bridge + trigger
;;   Iter 3: incremental update, ~3 Z3 re-verifications,
;;           removes bridge2 + trigger2
;;   Iter 4: fixpoint, 0 Z3 queries
;;
;; The shorter pddl/test/incremental-cascade/ exercises a
;; 2-step cascade. This 3-step variant additionally tests
;; that BFS-orphaning a multi-effect action propagates a
;; bound tightening into the next iteration's RPG.
;; ============================================================

(define (domain incremental-cascade-3)
  (:requirements :strips :numeric-fluents)
  (:predicates (g) (g2) (done))
  (:functions (x) (y) (z) (h))

  ;; Iter 1: removed (no goal-relevance, only modifies x).
  (:action waste
    :parameters ()
    :effect (assign (x) 100))

  ;; Iter 2: removed (achievement of y>=50 flips when x tightens to [0,0]).
  (:action bridge
    :parameters ()
    :precondition (g)
    :effect (assign (y) (x)))

  ;; Iter 2: BFS-orphaned (only consumer of g was bridge).
  ;; Multi-effect: also assigns h := 100 — this contribution survives
  ;; iter 2's RPG (trigger still in action set), but vanishes from
  ;; iter 3's RPG (trigger removed at end of iter 2).
  (:action trigger
    :parameters ()
    :effect (and (g) (assign (h) 100)))

  ;; Iter 3: removed (achievement of z>=50 flips when h tightens to [0,0]).
  (:action bridge2
    :parameters ()
    :precondition (g2)
    :effect (assign (z) (h)))

  ;; Iter 3: BFS-orphaned (only consumer of g2 was bridge2).
  (:action trigger2
    :parameters ()
    :effect (g2))

  ;; Constant alternatives keep goal RPG-reachable in every iteration,
  ;; so the achievers update path actually runs (no UNSOLVABLE short-circuit).
  (:action direct-y
    :parameters ()
    :effect (assign (y) 50))

  (:action direct-z
    :parameters ()
    :effect (assign (z) 50))

  (:action finish
    :parameters ()
    :precondition (and (>= (y) 50) (>= (z) 50))
    :effect (done))
)
