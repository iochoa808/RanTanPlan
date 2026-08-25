;; Test: EXISTS in a GOAL CONDITION.
;;
;; KEY PATTERN: (exists (?i - (number 0 4)) (= (read (arr) ?i) 7)) in (:goal ...).
;;   forall_goal tests forall in goals; this tests exists in goals.
;;   The planner must satisfy the existential by finding at least one cell = 7.
;;
;; Domain: 5-cell array; write value 7 into any one cell.
;; Initial: arr = [0,0,0,0,0].
;; Goal: (exists (?i - (number 0 4)) (= (read (arr) ?i) 7)).
;; Plan: write(2, 7)  — 1 step  (any i works).

(define (domain exists-goal)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx - (number 0 4)
        val - (number 0 9)
        arr - (array 5 val)
    )

    (:functions
        (cells) - arr
    )

    ;; Write value 7 into cell ?i.
    (:action write
        :parameters (?i - idx)
        :precondition (= (read (cells) ?i) 0)
        :effect (write (cells) (?i) 7)
    )
)
