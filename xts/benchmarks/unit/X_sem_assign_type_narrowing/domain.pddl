;; SEMANTIC BREAK: assigning from a wider bounded-integer type to a narrower
;; bounded-integer type, where the wider type's range is not a subrange of
;; the narrower type's range.
;;
;;   temp     - (number 15 25)   ; wider:  [15, 25]
;;   setpoint - (number 18 23)   ; narrower: [18, 23]
;;
;; `(assign (target) (current))` is only safe when current ∈ [18, 23].
;; When current ∈ {15, 16, 17} the result is below lo=18.
;; When current ∈ {24, 25} the result is above hi=23.
;;
;; SEMANTIC RULE: An assignment (assign f_narrow f_wide) is safe only when
;; the source type's entire range is a subrange of the target type.
;; [15,25] is NOT a subrange of [18,23] → the assignment should be rejected
;; OR the system must emit a range-narrowing warning.
;;
;; This is more subtle than direct overflow because BOTH types are valid
;; bounded integers; the violation only appears from the range mismatch
;; between them.  A naive check that "both sides are int-typed" would pass.
;;
;; Note: The C++ bound_tightening.cpp might silently accept this, treating
;; the assignment as clamped to [18,23].  That changes semantics silently.
;;
;; Expected: type/range error for the `sync_temp` action.

(define (domain X-sem-assign-type-narrowing)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        room     - object
        temp     - (number 15 25)   ; wider type
        setpoint - (number 18 23)   ; narrower type: [18,23] ⊂ [15,25]
    )

    (:functions
        (current  ?r - room) - temp      ; source: can hold values 15..25
        (target   ?r - room) - setpoint  ; destination: only valid for 18..23
    )

    ;; Range mismatch: current may be 15, 16, 17 (below target's lo=18)
    ;; or 24, 25 (above target's hi=23).
    (:action sync_temp
        :parameters (?r - room)
        :precondition ()
        :effect (assign (target ?r) (current ?r))
    )

    ;; copy_target_to_current removed: it would make current=target=20, satisfying
    ;; the goal (target=current) via a valid action, bypassing the type-narrowing test.
)
