;; BREAK TARGET: array.mk in :init with more elements than the declared size.
;;
;; `store` is declared as (array 3 val) — exactly 3 slots.  The problem file
;; initialises it with array.mk of 5 values.  This mismatch should be caught
;; as a size error rather than silently truncating or accepting extra elements.
;;
;; Expected: error reporting size mismatch between declared array length (3)
;;           and the number of values supplied in array.mk (5).

(define (domain X-array-mk-overcount)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 2)
        val   - (number 0 9)
        store - (array 3 val)    ; size is 3
    )

    (:functions
        (cells) - store
    )

    (:action bump
        :parameters (?i - slot)
        :precondition (< (read (cells) ?i) 9)
        :effect (write (cells) (?i) (+ (read (cells) ?i) 1))
    )
)
