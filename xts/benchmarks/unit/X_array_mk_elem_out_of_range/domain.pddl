;; BREAK TARGET: array.mk in :init supplies element values that are outside
;; the declared element type's [lo, hi] range.
;;
;; `store` is (array 3 val) where val is (number 0 5).
;; The problem file initialises it with (array.mk (0 99 -2)):
;;   - 99 exceeds the element type's hi = 5
;;   - -2 is below the element type's lo = 0
;;
;; A validator that only checks whether the array.mk list has the right
;; *count* of elements may silently accept values that violate the element
;; type bounds.  Each element should be individually range-checked.
;;
;; Expected: error for element 99 (> 5) and element -2 (< 0).

(define (domain X-array-mk-elem-out-of-range)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 2)
        val   - (number 0 5)    ; element range: [0, 5]
        store - (array 3 val)
    )

    (:functions
        (cells) - store
    )

    (:action bump
        :parameters (?i - slot)
        :precondition (< (read (cells) ?i) 5)
        :effect (write (cells) (?i) (+ (read (cells) ?i) 1))
    )
)
