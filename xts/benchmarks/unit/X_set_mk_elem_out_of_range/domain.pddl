;; BREAK TARGET: set.mk in :init contains an integer that is outside the
;; declared element type's range.
;;
;; `active` is (set altitude) where altitude is (number 0 9).
;; The problem initialises it with (set.mk (1 3 15)):
;;   - 15 exceeds altitude's hi = 9
;;
;; A validator that only checks whether set.mk elements are integers (vs
;; objects) may miss that 15 is out of the element type's declared range.
;; The same problem applies to a negative element in a non-negative type.
;;
;; `restricted` is initialised with (set.mk (-1 5)) — -1 is below lo = 0.
;;
;; Expected: error for element 15 (> 9) and element -1 (< 0).

(define (domain X-set-mk-elem-out-of-range)
    (:requirements :bounded-integers :sets)

    (:types
        altitude    - (number 0 9)    ; element range: [0, 9]
        altitudeset - (set altitude)
    )

    (:functions
        (active)     - altitudeset
        (restricted) - altitudeset
    )

    (:action enter
        :parameters (?a - altitude)
        :precondition (and
            (not (member ?a (active)))
            (not (member ?a (restricted)))
        )
        :effect (add ?a (active))
    )
)
