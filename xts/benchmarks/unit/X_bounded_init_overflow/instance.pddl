;; temperature is (number 0 5); 99 and -3 are both out of range.
;;
;; Goal: (= (temperature room1) 99)
;;
;; Achievable WITHOUT any action only if 99 is silently accepted as a valid
;; initial value.  No action can write 99 — `heat` increments by 1 with a
;; precondition (< temp 5), so the maximum reachable value is 5.
;;
;; Intended failure modes:
;;   CORRECT  — planner rejects 99 / -3 with a range error      → PASS
;;   BAD      — planner accepts 99, empty plan satisfies goal    → FAIL (reveals bug)
;;   CLAMPING — 99 clamped to 5, goal=99 is unreachable         → PASS (wrong reason)
(define (problem X-bounded-init-overflow-01)
    (:domain X-bounded-init-overflow)

    (:objects room1 room2 - room)

    (:init
        (= (temperature room1) 99)   ; too large — max is 5
        (= (temperature room2) -3)   ; too small — min is 0
    )

    (:goal (= (temperature room1) 99))
)
