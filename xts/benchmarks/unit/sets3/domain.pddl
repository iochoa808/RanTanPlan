(define (domain cold-lab)

    (:requirements :sets)

    (:types
        sample    - object
        sampleset - (set sample)   ;; set type: sampleset is a "set of sample"
        temp-lvl  - (number 0 5)   ;; bounded integer: 0 = cryo-cold, 5 = room temperature
    )

    (:functions
        (lab)     - sampleset   ;; samples currently in cold storage
        (archive) - sampleset   ;; reference archive — read-only
        (temp)    - temp-lvl    ;; current storage temperature
    )

    ;; ── manual item operations (require low temperature) ──────────────────────

    ;; Feature: member (precondition), add (effect), bounded-int precondition
    ;; Load a single sample when the unit is cold (temp ≤ 2).
    (:action load
        :parameters (?s - sample)
        :precondition (and
            (not (member ?s (lab)))
            (<= (temp) 2))
        :effect (add ?s (lab))
    )

    ;; Feature: member (precondition), remove (effect), bounded-int precondition
    ;; Unload a single sample when the unit is cold.
    (:action unload
        :parameters (?s - sample)
        :precondition (and
            (member ?s (lab))
            (<= (temp) 2))
        :effect (remove ?s (lab))
    )

    ;; ── temperature controls ───────────────────────────────────────────────────

    ;; Feature: bounded-int arithmetic effect (decrease by 1)
    (:action cool
        :parameters ()
        :precondition (> (temp) 0)
        :effect (assign (temp) (- (temp) 1))
    )

    ;; Feature: bounded-int arithmetic effect (increase by 1)
    (:action warm
        :parameters ()
        :precondition (< (temp) 5)
        :effect (assign (temp) (+ (temp) 1))
    )

    ;; ── bulk automated operations (no temperature gate) ───────────────────────

    ;; Feature: cardinality (precondition), intersect (effect)
    ;; When at least 3 samples are loaded, trim to only archive items.
    (:action trim_to_archive
        :parameters ()
        :precondition (>= (cardinality (lab)) 3)
        :effect (assign (lab) (intersect (lab) (archive)))
    )

    ;; Feature: subset (precondition), union (effect)
    ;; When lab ⊆ archive (no foreign samples), fill in all remaining archive items.
    (:action complete_from_archive
        :parameters ()
        :precondition (subset (lab) (archive))
        :effect (assign (lab) (union (lab) (archive)))
    )

    ;; Feature: disjoint (precondition), bounded-int precondition, union (effect)
    ;; Emergency restock when the lab holds no archive samples and is very cold (temp ≤ 1).
    (:action emergency_fill
        :parameters ()
        :precondition (and
            (disjoint (lab) (archive))
            (<= (temp) 1))
        :effect (assign (lab) (union (lab) (archive)))
    )
)
