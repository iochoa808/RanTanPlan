;; altitude is (number 0 9); 15 and -1 both violate the element type range.
;;
;; Goal: (member 15 (active))
;;
;; 15 is not a valid altitude value, so no action can ever `add` it — the
;; `enter` action takes `?a - altitude` which only grounds for 0..9.
;; The goal is reachable WITHOUT any action only if 15 is silently accepted
;; as a valid element in the initial set.
;;
;; Intended failure modes:
;;   CORRECT  — planner rejects 15 / -1 with a range error           → PASS
;;   BAD      — planner accepts 15, empty plan satisfies the goal     → FAIL (reveals bug)
;;   CLAMPING — 15 clamped to 9, member(15) is false, unreachable    → PASS (wrong reason)
(define (problem X-set-mk-elem-out-of-range-01)
    (:domain X-set-mk-elem-out-of-range)
    (:init
        (= (active)     (set.mk (1 3 15)))   ; 15 > 9
        (= (restricted) (set.mk (-1 5)))     ; -1 < 0
    )
    (:goal (member 15 (active)))
)
