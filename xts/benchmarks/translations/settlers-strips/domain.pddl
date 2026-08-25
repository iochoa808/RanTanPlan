;; PDDL-XTS translation of pddl/test/settlers-strips (Civilisation resource economy).
;; FEATURES: bounded integers + object fluents + sets.
;;   - (available ?r ?s) numeric fluent -> bounded-int (number 0 10); covers the
;;     maximum resource accumulation reachable in any short plan.
;;   - (space-in ?v) numeric fluent -> bounded-int (number 0 10); ship capacity = 10.
;;   - (labour), (resource-use), (pollution) -> bounded-int (number 0 30); these
;;     only increase and remain small in satisficing plans.
;;   - (housing ?p) -> bounded-int (number 0 10).
;;   - (is-at ?v ?p) total function for built vehicles -> object fluent
;;     (vehicle-at ?v) - place; nowhere is a domain constant sentinel for
;;     potential (not-yet-built) vehicles.
;;   - (connected-by-land ?p1 ?p2) static relation -> set fluent (land-conns ?p1).
;;   - (connected-by-rail ?p1 ?p2) dynamic relation -> set fluent (rail-conns ?p1);
;;     build-rail adds ?p2 to (rail-conns ?p1).
;;   - (connected-by-sea ?p1 ?p2) static relation -> set fluent (sea-conns ?p1).
;;   Metric dropped (satisficing).

(define (domain civ-xts)
    (:requirements :typing :bounded-integers :sets :object-fluents)
    (:types
        place vehicle - store
        resource - object
        placeset - (set place)
        res_t    - (number 0 10)   ;; resource amounts and vehicle space
        cnt_t    - (number 0 30)   ;; labour, resource-use, pollution
        hse_t    - (number 0 10)   ;; housing per place
    )
    (:constants
        timber wood coal stone iron ore - resource
        nowhere - place
    )
    (:predicates
        (woodland ?p - place)
        (mountain ?p - place)
        (metalliferous ?p - place)
        (by-coast ?p - place)
        (has-cabin ?p - place)
        (has-coal-stack ?p - place)
        (has-quarry ?p - place)
        (has-mine ?p - place)
        (has-sawmill ?p - place)
        (has-ironworks ?p - place)
        (has-docks ?p - place)
        (has-wharf ?p - place)
        (is-cart ?v - vehicle)
        (is-train ?v - vehicle)
        (is-ship ?v - vehicle)
        (potential ?v - vehicle)
    )
    (:functions
        (available ?r - resource ?s - store) - res_t
        (space-in ?v - vehicle)              - res_t
        (labour)                             - cnt_t
        (resource-use)                       - cnt_t
        (pollution)                          - cnt_t
        (housing ?p - place)                 - hse_t
        (vehicle-at ?v - vehicle)            - place
        (land-conns ?p - place)              - placeset
        (rail-conns ?p - place)              - placeset
        (sea-conns ?p - place)               - placeset
    )

    ;; ---- A.1: Loading and unloading ----

    (:action load
        :parameters (?v - vehicle ?p - place ?r - resource)
        :precondition (and (= (vehicle-at ?v) ?p)
                           (> (available ?r ?p) 0)
                           (> (space-in ?v) 0))
        :effect (and (decrease (space-in ?v) 1)
                     (increase (available ?r ?v) 1)
                     (decrease (available ?r ?p) 1)
                     (increase (labour) 1))
    )

    (:action unload
        :parameters (?v - vehicle ?p - place ?r - resource)
        :precondition (and (= (vehicle-at ?v) ?p)
                           (> (available ?r ?v) 0))
        :effect (and (increase (space-in ?v) 1)
                     (decrease (available ?r ?v) 1)
                     (increase (available ?r ?p) 1)
                     (increase (labour) 1))
    )

    ;; ---- A.2: Moving vehicles ----

    (:action move-cart
        :parameters (?v - vehicle ?p1 ?p2 - place)
        :precondition (and (is-cart ?v)
                           (member ?p2 (land-conns ?p1))
                           (= (vehicle-at ?v) ?p1))
        :effect (and (assign (vehicle-at ?v) ?p2)
                     (increase (labour) 2))
    )

    (:action move-train
        :parameters (?v - vehicle ?p1 ?p2 - place)
        :precondition (and (is-train ?v)
                           (member ?p2 (rail-conns ?p1))
                           (= (vehicle-at ?v) ?p1)
                           (>= (available coal ?v) 1))
        :effect (and (assign (vehicle-at ?v) ?p2)
                     (decrease (available coal ?v) 1)
                     (increase (pollution) 1))
    )

    (:action move-ship
        :parameters (?v - vehicle ?p1 ?p2 - place)
        :precondition (and (is-ship ?v)
                           (member ?p2 (sea-conns ?p1))
                           (= (vehicle-at ?v) ?p1)
                           (>= (available coal ?v) 2))
        :effect (and (assign (vehicle-at ?v) ?p2)
                     (decrease (available coal ?v) 2)
                     (increase (pollution) 2))
    )

    ;; ---- B.1: Building structures ----

    (:action build-cabin
        :parameters (?p - place)
        :precondition (woodland ?p)
        :effect (and (increase (labour) 1) (has-cabin ?p))
    )

    (:action build-quarry
        :parameters (?p - place)
        :precondition (mountain ?p)
        :effect (and (increase (labour) 2) (has-quarry ?p))
    )

    (:action build-coal-stack
        :parameters (?p - place)
        :precondition (>= (available timber ?p) 1)
        :effect (and (increase (labour) 2)
                     (decrease (available timber ?p) 1)
                     (has-coal-stack ?p))
    )

    (:action build-sawmill
        :parameters (?p - place)
        :precondition (>= (available timber ?p) 2)
        :effect (and (increase (labour) 2)
                     (decrease (available timber ?p) 2)
                     (has-sawmill ?p))
    )

    (:action build-mine
        :parameters (?p - place)
        :precondition (and (metalliferous ?p) (>= (available wood ?p) 2))
        :effect (and (increase (labour) 3)
                     (decrease (available wood ?p) 2)
                     (has-mine ?p))
    )

    (:action build-ironworks
        :parameters (?p - place)
        :precondition (and (>= (available stone ?p) 2) (>= (available wood ?p) 2))
        :effect (and (increase (labour) 3)
                     (decrease (available stone ?p) 2)
                     (decrease (available wood ?p) 2)
                     (has-ironworks ?p))
    )

    (:action build-docks
        :parameters (?p - place)
        :precondition (and (by-coast ?p)
                           (>= (available stone ?p) 2)
                           (>= (available wood ?p) 2))
        :effect (and (decrease (available stone ?p) 2)
                     (decrease (available wood ?p) 2)
                     (increase (labour) 2)
                     (has-docks ?p))
    )

    (:action build-wharf
        :parameters (?p - place)
        :precondition (and (has-docks ?p)
                           (>= (available stone ?p) 2)
                           (>= (available iron ?p) 2))
        :effect (and (decrease (available stone ?p) 2)
                     (decrease (available iron ?p) 2)
                     (increase (labour) 2)
                     (has-wharf ?p))
    )

    ;; build-rail adds rail connectivity in one direction.
    (:action build-rail
        :parameters (?p1 ?p2 - place)
        :precondition (and (member ?p2 (land-conns ?p1))
                           (>= (available wood ?p1) 1)
                           (>= (available iron ?p1) 1))
        :effect (and (decrease (available wood ?p1) 1)
                     (decrease (available iron ?p1) 1)
                     (increase (labour) 2)
                     (add ?p2 (rail-conns ?p1)))
    )

    (:action build-house
        :parameters (?p - place)
        :precondition (and (>= (available wood ?p) 1) (>= (available stone ?p) 1))
        :effect (and (increase (housing ?p) 1)
                     (decrease (available wood ?p) 1)
                     (decrease (available stone ?p) 1))
    )

    ;; ---- B.2: Building vehicles ----

    (:action build-cart
        :parameters (?p - place ?v - vehicle)
        :precondition (and (>= (available timber ?p) 1) (potential ?v))
        :effect (and (decrease (available timber ?p) 1)
                     (assign (vehicle-at ?v) ?p)
                     (is-cart ?v)
                     (not (potential ?v))
                     (assign (space-in ?v) 1)
                     (assign (available timber ?v) 0)
                     (assign (available wood ?v) 0)
                     (assign (available coal ?v) 0)
                     (assign (available stone ?v) 0)
                     (assign (available iron ?v) 0)
                     (assign (available ore ?v) 0)
                     (increase (labour) 1))
    )

    (:action build-train
        :parameters (?p - place ?v - vehicle)
        :precondition (and (potential ?v) (>= (available iron ?p) 2))
        :effect (and (decrease (available iron ?p) 2)
                     (assign (vehicle-at ?v) ?p)
                     (is-train ?v)
                     (not (potential ?v))
                     (assign (space-in ?v) 5)
                     (assign (available timber ?v) 0)
                     (assign (available wood ?v) 0)
                     (assign (available coal ?v) 0)
                     (assign (available stone ?v) 0)
                     (assign (available iron ?v) 0)
                     (assign (available ore ?v) 0)
                     (increase (labour) 2))
    )

    (:action build-ship
        :parameters (?p - place ?v - vehicle)
        :precondition (and (potential ?v) (>= (available iron ?p) 4))
        :effect (and (has-wharf ?p)
                     (decrease (available iron ?p) 4)
                     (assign (vehicle-at ?v) ?p)
                     (is-ship ?v)
                     (not (potential ?v))
                     (assign (space-in ?v) 10)
                     (assign (available timber ?v) 0)
                     (assign (available wood ?v) 0)
                     (assign (available coal ?v) 0)
                     (assign (available stone ?v) 0)
                     (assign (available iron ?v) 0)
                     (assign (available ore ?v) 0)
                     (increase (labour) 3))
    )

    ;; ---- C.1: Obtaining raw resources ----

    (:action fell-timber
        :parameters (?p - place)
        :precondition (has-cabin ?p)
        :effect (and (increase (available timber ?p) 1) (increase (labour) 1))
    )

    (:action break-stone
        :parameters (?p - place)
        :precondition (has-quarry ?p)
        :effect (and (increase (available stone ?p) 1)
                     (increase (labour) 1)
                     (increase (resource-use) 1))
    )

    (:action mine-ore
        :parameters (?p - place)
        :precondition (has-mine ?p)
        :effect (and (increase (available ore ?p) 1)
                     (increase (resource-use) 2))
    )

    ;; ---- C.2: Refining resources ----

    (:action burn-coal
        :parameters (?p - place)
        :precondition (and (has-coal-stack ?p) (>= (available timber ?p) 1))
        :effect (and (decrease (available timber ?p) 1)
                     (increase (available coal ?p) 1)
                     (increase (pollution) 1))
    )

    (:action saw-wood
        :parameters (?p - place)
        :precondition (and (has-sawmill ?p) (>= (available timber ?p) 1))
        :effect (and (decrease (available timber ?p) 1)
                     (increase (available wood ?p) 1))
    )

    (:action make-iron
        :parameters (?p - place)
        :precondition (and (has-ironworks ?p)
                           (>= (available ore ?p) 1)
                           (>= (available coal ?p) 2))
        :effect (and (decrease (available ore ?p) 1)
                     (decrease (available coal ?p) 2)
                     (increase (available iron ?p) 1)
                     (increase (pollution) 2))
    )
)