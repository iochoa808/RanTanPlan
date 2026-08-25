;; board = [0, 0], guard is NOT set.
;;
;; fire() executes:
;;   slot 1 ← 3   (unconditional, always applies)
;;   slot 0 ← 9   (conditional on guard — guard is false, so this does NOT apply)
;;
;; After fire: board = [0, 3].
;;
;; Goal: board[1]=3 AND board[0]=0
;;   board[0]=0 verifies the conditional write was correctly skipped.
;;
;; Expected plan (1 step): fire()

(define (problem cond-noop-01)
    (:domain cond-noop)

    (:init
        (= (board) (array.mk (0 0)))
        ;; (guard) is NOT in init → false
    )

    (:goal
        (and
            (= (read (board) (1)) 3)
            (= (read (board) (0)) 0)
        )
    )
)
