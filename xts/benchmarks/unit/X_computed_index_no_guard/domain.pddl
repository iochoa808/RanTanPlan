;; Test: computed array index without an out-of-bounds guard.
;;
;; KEY MECHANISM: IPAR instantiates ?i=0 (the only grounding of slot=(number 0 0))
;; and evaluates the effect-value index (+ 0 1) = 1.  The array has size 1, so
;; index 1 is out of bounds.  IPAR prunes the grounding, leaving an empty action
;; set.  The goal is trivially unreachable → UNSOLVABLE immediately.
;;
;; The correct fix for such domains is to guard the computed index with a bound
;; check in the precondition, e.g. (< ?i max_valid_index), so the OOB grounding
;; is excluded by the guard rather than silently by IPAR.
;;
;; Expected: UNSOLVABLE (fast — empty action set after IPAR pruning).

(define (domain X-computed-index-no-guard)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 0)    ;; only index 0 — so (+ 0 1) = 1 is statically OOB
        val   - (number 0 9)
        store - (array 1 val)   ;; size 1 → only cell 0 is valid
    )

    (:functions
        (cells) - store
    )

    ;; ?i = 0 (the only grounding): reads cells[(+ 0 1)] = cells[1] → OOB for size-1 array.
    ;; No guard prevents this; IPAR prunes the action entirely.
    (:action shift_right
        :parameters (?i - slot)
        :precondition ()
        :effect (write (cells) (?i) (read (cells) (+ ?i 1)))
    )
)
