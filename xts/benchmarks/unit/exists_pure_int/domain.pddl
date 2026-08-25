;; Test: EXISTS with a PURE BOUNDED-INT body — no array reads, no set membership.
;;
;; KEY PATTERN: (exists (?i - (number 0 3)) (> (threshold) ?i))
;;   All existing exists tests read an array or check set membership in the body.
;;   This exercises existential quantification over integers where the body is
;;   a plain arithmetic comparison between a fluent and the quantified variable.
;;
;; Semantics: the precondition holds when threshold > some integer in [0,3],
;;   i.e. when threshold >= 1.  Forces the planner to raise threshold first.
;;
;; Domain: counter [0,9]; inc raises it by 1.
;; Initial: threshold=0; action inc usable only when exists i in [0,3]: threshold>i.
;; Goal: done=True.
;; Plan: inc() × 1 step → threshold=1 → exists(i=0: 1>0) ✓ → unlock() → done.

(define (domain exists-pure)
    (:requirements :typing :adl :bounded-integers)

    (:types
        cnt - (number 0 9)
    )

    (:predicates
        (done)
    )

    (:functions
        (threshold) - cnt
    )

    ;; Raise threshold by 1.
    (:action inc
        :parameters ()
        :precondition (< (threshold) 9)
        :effect (assign (threshold) (+ (threshold) 1))
    )

    ;; Unlock: fires when there exists i in [0..3] such that threshold > i.
    (:action unlock
        :parameters ()
        :precondition (and
            (not (done))
            (exists (?i - (number 0 3)) (> (threshold) ?i))
        )
        :effect (done)
    )
)
