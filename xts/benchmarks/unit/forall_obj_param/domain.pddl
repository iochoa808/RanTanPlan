;; Test: FORALL (constant range) effect in an action whose ONLY parameter is
;;       a REGULAR OBJECT TYPE — not a bounded integer.
;;
;; KEY PATTERN:
;;   (:action wipe
;;       :parameters (?a - agent)    ; regular object, NOT (number lo hi)
;;       :effect (forall (?i - (number 0 3))
;;                   (write (board) (?i) 0)))
;;
;; WHY THIS MATTERS:
;;   IPAR (Python-side compiler) expands bounded-integer action parameters into
;;   one action copy per integer value.  When the action has NO integer parameter,
;;   IPAR creates exactly one template with empty `int_param_map` and calls
;;   `_update_range_vars` with no substitutions — the forall bounds (0 and 3)
;;   are evaluated as-is.
;;
;;   If IPAR is disabled (--up-compilers none), the forall effect reaches C++
;;   as a `range_var(i, 0, 3)` node inside a quantified EffectExpression.
;;   ForallGrounderPass must then enumerate i in [0, 3] and emit four concrete
;;   WRITE effects.
;;
;;   This test is DIFFERENT from:
;;     forall_const_range  — :parameters ()  — no parameter at all
;;     forall_param_range  — :parameters (?n - (number 0 4))  — bounded int param
;;                           where the forall BOUND depends on the action param
;;   Here the object param (?a) is completely independent of the forall range.
;;
;; PLAN:
;;   authorize(admin), wipe(admin)  — 2 steps.
;;
;; If the forall expands correctly, all 4 cells become 0 and the goal is reached.
;; If any forall iteration is skipped, at least one cell stays non-zero and the
;; planner cannot satisfy the goal.

(define (domain forall-obj-param)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        agent - object
        idx - (number 0 3)
        val - (number 0 9)
        board-t - (array 4 val)
    )

    (:predicates
        (authorized ?a - agent)
    )

    (:functions
        (board) - board-t
    )

    ;; Grant authorization to an agent (prerequisite for wiping).
    ;; Exists so the object parameter ?a is semantically load-bearing:
    ;; only the authorized agent can wipe, which forces the grounder to
    ;; try all agents and check the predicate.
    (:action authorize
        :parameters (?a - agent)
        :precondition (not (authorized ?a))
        :effect (authorized ?a)
    )

    ;; Wipe all four cells to 0.
    ;; ?a is a plain object parameter — NOT a bounded integer.
    ;; The forall range (number 0 3) is a ground constant range: lo=0, hi=3.
    ;; Neither IPAR nor ForallGrounderPass may substitute ?a into the bounds —
    ;; they must simply enumerate i in {0, 1, 2, 3} and emit four WRITE effects.
    (:action wipe
        :parameters (?a - agent)
        :precondition (authorized ?a)
        :effect (forall (?i - (number 0 3))
                    (write (board) (?i) 0))
    )
)