;; BREAK TARGET: ARRAY OF ARRAY TYPE — array whose element type is another array.
;;
;; The N-D array syntax requires the element type to be a scalar
;; (bounded integer, set, or object type).  Using another array type as
;; the element type is not supported and should be rejected.
;;
;;   inner - (array 2 val)    ← first-class array type
;;   outer - (array 3 inner)  ← ERROR: inner is an array, not a scalar
;;
;; The flat N-D form (array 3 2 val) is the correct way to declare a
;; 2D array and must be used instead of nesting explicit array types.
;;
;; Expected: parse/semantic error — array element type is itself an array type.

(define (domain X-array-of-array)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val   - (number 0 9)
        inner - (array 2 val)
        ;; Error: element type is another array type, not a scalar
        outer - (array 3 inner)
    )

    (:predicates (done))

    (:functions
        (data) - outer
    )

    (:action mark
        :parameters ()
        :precondition (not (done))
        :effect (done)
    )
)
