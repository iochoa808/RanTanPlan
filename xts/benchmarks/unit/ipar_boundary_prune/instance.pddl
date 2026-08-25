;; Starting at position 0, move right to position 3 while logging each step.
;;
;; Expected plan (3 steps):
;;   step_right_1 : cursor 0→1, log[0] := 0
;;   step_right_2 : cursor 1→2, log[1] := 1
;;   step_right_3 : cursor 2→3, log[2] := 2   ← satisfies (= (read (log) 2) 2)
;;
;; The boundary actions step_right_0 and step_left_3 are pruned by IPAR before
;; reaching C++.  Without that pruning, SemanticValidationPass would reject
;; step_right_0's write to index -1 and step_left_3's write to index 4.

(define (problem ipar-boundary-prune-01)
    (:domain ipar-boundary-prune)
    (:objects)
    (:init
        (= (cursor) 0)
        (= (log) (array.mk (9 9 9 9)))
    )
    (:goal
        (and
            (= (cursor) 3)
            (= (read (log) 2) 2)
        )
    )
)