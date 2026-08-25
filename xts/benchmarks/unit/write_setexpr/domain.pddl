;; Test: write a SET EXPRESSION result (union of two reads) into an array cell.
;;
;; KEY PATTERN: (write (cells) (?dst) (union (read (cells) ?dst) (read (cells) ?src)))
;; The value being written is not a plain read-copy but an evaluated set expression.
;;
;; Domain: 3-slot store of integer sets; merge action unions two cells.

(define (domain set-merger)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        slot   - (number 0 2)
        val    - (number 0 4)
        valset - (set val)
        store  - (array 3 valset)
    )

    (:functions
        (cells) - store
    )

    ;; Merge: union cell ?src into cell ?dst.
    (:action merge_into
        :parameters (?src - slot ?dst - slot)
        :precondition (not (= ?src ?dst))
        :effect (write (cells) (?dst)
                       (union (read (cells) ?dst) (read (cells) ?src)))
    )
)
