(define (domain counter)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        count-val - (number 0 5)
        inc-val   - (number 1 5)   ;; increment amount: [1,5]
        set-val   - (number 0 4)   ;; set_counter limited to [0,4] to force use of inc
    )

    (:functions
        (counter) - count-val
    )

    ;; Assign counter = ?v directly (param-value assignment).
    ;; Limited to set-val [0,4] so goal=5 cannot be reached without inc.
    (:action set-counter
        :parameters (?v - set-val)
        :precondition (< (counter) ?v)
        :effect (assign (counter) ?v)
    )

    ;; Assign counter = counter + ?v (param-arithmetic assignment).
    (:action inc
        :parameters (?v - inc-val)
        :precondition (<= (+ (counter) ?v) 5)
        :effect (assign (counter) (+ (counter) ?v))
    )
)
