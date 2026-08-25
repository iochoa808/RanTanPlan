(define (domain area-calculator)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        width-t  - (number 1 3)
        height-t - (number 1 3)
        area-t   - (number 0 9)   ;; max = 3*3 = 9
    )

    (:functions
        (width)  - width-t
        (height) - height-t
        (area)   - area-t
    )

    ;; Raise height to ?h.
    (:action set-height
        :parameters (?h - height-t)
        :precondition (< (height) ?h)
        :effect (assign (height) ?h)
    )

    ;; Compute area = width * height.
    (:action compute
        :parameters ()
        :precondition (= (area) 0)
        :effect (assign (area) (* (width) (height)))
    )
)
