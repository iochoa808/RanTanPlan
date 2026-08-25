(define (domain autonomous-drone-corridor)

    (:requirements :sets)

    ;; ──────────────────────────────────────────────────────────────────────────
    ;; TYPES
    ;; ──────────────────────────────────────────────────────────────────────────

    (:types
        altitude     - (number 0 9)           ;; bounded flight levels
        altitudeset  - (set altitude)         ;; sets of flight levels

        dronecount   - (number 0 5)           ;; bounded congestion metric
    )

    ;; ──────────────────────────────────────────────────────────────────────────
    ;; STATE
    ;; ──────────────────────────────────────────────────────────────────────────

    (:functions
        (active-layers)     - altitudeset     ;; currently used air layers
        (restricted-layers) - altitudeset     ;; no-fly or hazardous layers
        (charging-layers)   - altitudeset     ;; layers near charging hubs

        (ceiling)           - altitude        ;; current legal max altitude
        (traffic-load)      - dronecount      ;; congestion estimate
    )

    ;; ──────────────────────────────────────────────────────────────────────────
    ;; FLIGHT OPERATIONS
    ;; ──────────────────────────────────────────────────────────────────────────

    ;; A drone may enter an altitude layer if:
    ;;  - the layer is not already occupied
    ;;  - it is below the current ceiling
    ;;  - it is not restricted
    ;;
    ;; Features:
    ;;   member, bounded comparisons, set negation, add
    (:action enter-layer
        :parameters (?a - altitude)
        :precondition (and
            (<= ?a (ceiling))
            (not (member ?a (active-layers)))
            (not (member ?a (restricted-layers))))
        :effect (and
            (add ?a (active-layers))
            (assign (traffic-load) (+ (traffic-load) 1)))
    )

    ;; Remove a drone from an altitude layer.
    ;;
    ;; Features:
    ;;   member, remove, bounded arithmetic
    (:action exit-layer
        :parameters (?a - altitude)
        :precondition (member ?a (active-layers))
        :effect (and
            (remove ?a (active-layers))
            (assign (traffic-load) (- (traffic-load) 1)))
    )

    ;; ──────────────────────────────────────────────────────────────────────────
    ;; AIRSPACE REGULATION
    ;; ──────────────────────────────────────────────────────────────────────────

    ;; Increase maximum allowed altitude during good conditions.
    ;;
    ;; Features:
    ;;   bounded integer arithmetic
    (:action raise-ceiling
        :parameters ()
        :precondition (< (ceiling) 9)
        :effect (assign (ceiling) (+ (ceiling) 1))
    )

    ;; Reduce allowed altitude during storms or emergencies.
    ;;
    ;; Features:
    ;;   bounded integer arithmetic
    (:action lower-ceiling
        :parameters ()
        :precondition (> (ceiling) 0)
        :effect (assign (ceiling) (- (ceiling) 1))
    )

    ;; ──────────────────────────────────────────────────────────────────────────
    ;; SAFETY AUTOMATION
    ;; ──────────────────────────────────────────────────────────────────────────

    ;; If traffic becomes dense, automatically keep only charging-safe layers.
    ;;
    ;; Features:
    ;;   cardinality, intersect
    (:action congestion-filter
        :parameters ()
        :precondition (>= (cardinality (active-layers)) 4)
        :effect
            (assign (active-layers)
                (intersect (active-layers) (charging-layers)))
    )

    ;; If all active layers are safe charging layers,
    ;; automatically activate all charging corridors.
    ;;
    ;; Features:
    ;;   subset, union
    (:action activate-charging-grid
        :parameters ()
        :precondition
            (subset (active-layers) (charging-layers))
        :effect
            (assign (active-layers)
                (union (active-layers) (charging-layers)))
    )

    ;; Emergency evacuation routing:
    ;; if active and restricted layers are fully disjoint,
    ;; merge them into a globally coordinated corridor map.
    ;;
    ;; Features:
    ;;   disjoint, union
    (:action coordinated-reroute
        :parameters ()
        :precondition
            (disjoint (active-layers) (restricted-layers))
        :effect
            (assign (active-layers)
                (union (active-layers) (restricted-layers)))
    )

    ;; ──────────────────────────────────────────────────────────────────────────
    ;; ADVANCED SET OPERATIONS
    ;; ──────────────────────────────────────────────────────────────────────────

    ;; Clear all hazardous layers from the active flight map.
    ;;
    ;; Features:
    ;;   difference
    (:action purge-hazards
        :parameters ()
        :precondition ()
        :effect
            (assign (active-layers)
                (difference (active-layers) (restricted-layers)))
    )

    ;; Stabilize traffic by forcing the active set to exactly match
    ;; the charging infrastructure layers.
    ;;
    ;; Features:
    ;;   equality between sets
    (:action synchronize-grid
        :parameters ()
        :precondition
            (= (traffic-load) 0)
        :effect
            (assign (active-layers) (charging-layers))
    )
)