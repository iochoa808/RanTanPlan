;; Test: set predicates (cardinality, subset) applied to values READ from an array.
;;
;; KEY PATTERNS:
;;   (>= (cardinality (read (cells) ?s)) 3)   — cardinality of an array-read set
;;   (not (subset (read (cells) ?dst) (read (cells) ?src)))  — subset of two reads
;;   (write ... (intersect (read ...) (read ...)))  — intersect write
;;
;; Domain: 3-slot store of int-sets; count "large" cells and trim overlapping ones.

(define (domain cell-inspector)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        slot  - (number 0 2)
        val   - (number 0 4)
        vset  - (set val)
        store - (array 3 vset)
        count - (number 0 3)
    )

    (:functions
        (cells) - store
        (big)   - count   ; cells confirmed to have >= 3 elements
    )

    ;; Increment counter for a cell that has >= 3 elements.
    (:action count_big
        :parameters (?s - slot)
        :precondition (and
            (< (big) 3)
            (>= (cardinality (read (cells) ?s)) 3)
        )
        :effect (assign (big) (+ (big) 1))
    )

    ;; Trim cell ?dst to only values also in cell ?src (intersect-write).
    ;; Precondition: ?dst is NOT already a subset of ?src.
    (:action trim_to
        :parameters (?src - slot ?dst - slot)
        :precondition (and
            (not (= ?src ?dst))
            (not (subset (read (cells) ?dst) (read (cells) ?src)))
        )
        :effect (write (cells) (?dst)
                       (intersect (read (cells) ?dst) (read (cells) ?src)))
    )
)
