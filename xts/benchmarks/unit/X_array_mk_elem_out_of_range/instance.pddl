;; cells[0] is initialised to 99 — beyond val's hi=5.
;;
;; Goal: (= (read (cells) 0) 99)
;;
;; The goal is reachable WITHOUT any action — only if 99 is silently accepted
;; as a valid state value in the initial array.  No action can write 99
;; (bump only increments up to 5), so the plan would be the empty plan.
;;
;; Intended failure modes:
;;   CORRECT  — planner rejects init with an out-of-range error   → PASS
;;   BAD      — planner accepts 99, reads it back, empty plan found → FAIL (reveals bug)
;;   CLAMPING — planner silently clamps 99 to 5, goal unachievable → PASS (but for wrong reason)
;;
;; Using -2 in cell 1 adds a second violation (below lo=0).
(define (problem X-array-mk-elem-out-of-range-01)
    (:domain X-array-mk-elem-out-of-range)
    (:init
        (= (cells) (array.mk (99 -2 0)))
    )
    (:goal (= (read (cells) 0) 99))
)
