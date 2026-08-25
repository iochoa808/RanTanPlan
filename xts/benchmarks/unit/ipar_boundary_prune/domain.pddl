;; IPAR boundary-action pruning test
;;
;; Verifies that IPAR discards dead boundary actions before they reach the C++
;; SemanticValidationPass, which would otherwise raise an out-of-bounds error on
;; the array write index even though the action is unreachable at runtime.
;;
;; The pattern (identical to 15-puzzle's move_up/move_down):
;;
;;   step_right(?i - pos):
;;     precondition : (= (cursor) (- ?i 1))
;;     effect       : write ((log) ((- ?i 1))) (- ?i 1)   ; index = i-1
;;
;;   When ?i=0 → precondition (= cursor -1) is always false (cursor ∈ [0,3])
;;             → write index   -1  is out of bounds for a size-4 array
;;   IPAR must prune step_right_0 before C++ sees it.
;;
;;   step_left(?i - pos):
;;     precondition : (= (cursor) (+ ?i 1))
;;     effect       : write ((log) ((+ ?i 1))) (+ ?i 1)   ; index = i+1
;;
;;   When ?i=3 → precondition (= cursor 4) is always false (cursor ∈ [0,3])
;;             → write index   4  is out of bounds for a size-4 array
;;   IPAR must prune step_left_3 before C++ sees it.
;;
;; Non-boundary instantiations (step_right_1..3, step_left_0..2) are valid and
;; the planner should find a plan using them.

(define (domain ipar-boundary-prune)
    (:requirements :typing :numeric-fluents :bounded-integers :arrays)

    (:types
        pos   - (number 0 3)    ; cursor positions 0..3
        val   - (number 0 9)    ; values stored in log (source positions land in 0..2)
        log-t - (array 4 val)   ; 4-cell log array
    )

    (:functions
        (cursor) - pos           ; current cursor position
        (log)    - log-t         ; log[i] records the source position used to reach i
    )

    ;; Move cursor right from position (i-1) to i, recording the move in the log.
    ;; Boundary case step_right_0: precondition (= cursor -1) is infeasible;
    ;; write index (- 0 1) = -1 is out of bounds.  Pruned by IPAR.
    ;; Valid for ?i = 1, 2, 3.
    (:action step_right
        :parameters (?i - pos)
        :precondition (= (cursor) (- ?i 1))
        :effect (and
            (assign (cursor) ?i)
            (write ((log) ((- ?i 1))) (- ?i 1))
        )
    )

    ;; Move cursor left from position (i+1) to i, recording the move in the log.
    ;; Boundary case step_left_3: precondition (= cursor 4) is infeasible;
    ;; write index (+ 3 1) = 4 is out of bounds.  Pruned by IPAR.
    ;; Valid for ?i = 0, 1, 2.
    (:action step_left
        :parameters (?i - pos)
        :precondition (= (cursor) (+ ?i 1))
        :effect (and
            (assign (cursor) ?i)
            (write ((log) ((+ ?i 1))) (+ ?i 1))
        )
    )
)