;; Test: FORALL (constant range) in PRECONDITION + EFFECT, where the BODY
;;       contains ARRAY READS and ARRAY WRITES respectively.
;;
;; KEY PATTERNS:
;;   precondition: (forall (?i - (number 0 3)) (= (read (cells) ?i) 0))
;;   effect:       (forall (?i - (number 0 3)) (write (cells) (?i) 0))
;;
;; Combines the constant-range forall (forall_const_range) with the
;; array-fluent body (forall_param_range) into one test.
;; The precondition forall reads from the array; the effect forall writes to it.
;;
;; Domain: a 4-slot array; zero all slots at once, gated on a predicate.

(define (domain forall-array-body)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx - (number 0 3)
        val - (number 0 5)
        arr - (array 4 val)
    )

    (:predicates
        (gate_open)
    )

    (:functions
        (cells) - arr
    )

    ;; Open the gate (prerequisite for reset).
    (:action open_gate
        :parameters ()
        :precondition (not (gate_open))
        :effect (gate_open)
    )

    ;; Zero every slot — only if gate is open AND every slot is currently > 0.
    (:action reset_all
        :parameters ()
        :precondition (and
            (gate_open)
            (forall (?i - (number 0 3)) (> (read (cells) ?i) 0))
        )
        :effect (forall (?i - (number 0 3))
                    (write (cells) (?i) 0))
    )
)
