(define (problem drone-corridor-scenario-01)

    (:domain autonomous-drone-corridor)

    ;; ──────────────────────────────────────────────────────────────────────────
    ;; INITIAL STATE
    ;; ──────────────────────────────────────────────────────────────────────────

    (:init

        ;; Initial active drone layers
        (= (active-layers) (set.mk (1 3)))

        ;; Hazardous weather corridor
        (= (restricted-layers) (set.mk (5 7)))

        ;; Safe charging lanes
        (= (charging-layers) (set.mk (2 3 4)))

        ;; Current altitude ceiling
        (= (ceiling) 4)

        ;; Current congestion estimate
        (= (traffic-load) 2)
    )

    ;; ──────────────────────────────────────────────────────────────────────────
    ;; GOAL
    ;; ──────────────────────────────────────────────────────────────────────────

    (:goal
        (and

            ;; Ensure every active layer is charging-safe
            (subset (active-layers) (charging-layers))

            ;; Avoid hazardous overlap
            (disjoint (active-layers) (restricted-layers))

            ;; Reach higher operational capacity
            (>= (ceiling) 6)

            ;; Maintain low traffic
            (<= (traffic-load) 3)

            ;; Ensure layer 4 becomes active
            (member 4 (active-layers))
        )
    )
)