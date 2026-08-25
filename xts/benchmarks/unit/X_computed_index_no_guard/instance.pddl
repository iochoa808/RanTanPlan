;; cells = [9].  Goal: cells[0] = 5.
;;
;; shift_right is the only action; its sole grounding (?i=0) reads cells[1] which
;; is out of bounds for a size-1 array.  IPAR prunes it, leaving zero actions.
;; With no actions the state is always [9], so cells[0]=5 is unreachable.
;;
;; Expected: UNSOLVABLE (immediately — empty action set after IPAR pruning).

(define (problem X-computed-index-no-guard-01)
    (:domain X-computed-index-no-guard)

    (:init
        (= (cells) (array.mk (9)))
    )

    (:goal (= (read (cells) 0) 5))
)
