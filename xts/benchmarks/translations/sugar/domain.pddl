;; PDDL-XTS translation of pddl/test/sugar (supply-chain).
;; FEATURES: bounded integers + object fluents + sets.
;;   - (at-location ?d ?l) total function for loaders -> object fluent
;;     (loader-at ?d) - location; drive_truck and (implicitly) crane actions use it.
;;   - (connected ?l1 ?l2) static binary relation -> set fluent (connects ?l1) - locset.
;;   - All numeric fluents -> bounded integers:
;;       has-resource  : (number 0 15)  sugar-cane at a mill (start 5, up to +10 from farm)
;;       max-produce   : (number 0 5)   static per-mill max batch (=3 in instance)
;;       max-changing  : (number 0 2)   brand-change budget (decremented by change-product)
;;       in-storage    : (number 0 10)  sugar stock at a location
;;       truck-cap     : (number 0 5)   remaining truck capacity
;;       in-truck-sugar: (number 0 5)   sugar aboard a truck per brand
;;       cran-cap      : (number 0 5)   crane load size (static, =2 in instance)
;;       svc_t         : (number 0 5)   service-time / max-service-time for cranes
;;       unf_t         : (number 0 2)   unharvest-field (decremented by sugar-cane-farm)
;;       cost-process  : (number 0 5)   static per-mill production cost (=1 in instance)
;;   - Purely metric accumulators (mill-cost, inventory-cost, handling-cost) are
;;     dropped: satisficing planning ignores them and the goal does not mention them.
;;   - Functions declared in the original but not referenced in any action
;;     (total-distance, distance, process-cost, resource-use, labour-cost,
;;     changing-product) are also dropped.

(define (domain supply-chain-xts)
    (:requirements :typing :bounded-integers :sets :object-fluents)
    (:types
        sugar location loader - object
        brand raw-cane        - sugar
        mill depot            - location
        truck crane           - loader
        farm field
        locset  - (set location)
        res_t   - (number 0 15)   ;; sugar-cane resource amounts
        stk_t   - (number 0 10)   ;; in-storage / truck carry amounts
        cap_t   - (number 0 5)    ;; crane capacity, cost-process, service time, max-produce
        chg_t   - (number 0 2)    ;; max-changing, unharvest-field
    )
    (:predicates
        (available ?m - mill)
        (produce ?m - mill ?b - brand)
        (current-process ?m - mill ?b - brand)
        (change-process ?b1 ?b2 - brand)
        (place-order ?r - raw-cane ?m - mill)
        (transport-to ?r - raw-cane ?m - mill)
        (busy ?m - mill)
        (ready-crane ?c - crane)
        (service-crane ?c - crane)
    )
    (:functions
        (loader-at ?d - loader)               - location
        (connects ?l - location)              - locset
        (has-resource ?r - raw-cane ?m - mill) - res_t
        (max-produce ?m - mill)               - cap_t
        (max-changing ?m - mill)              - chg_t
        (in-storage ?l - location ?b - brand) - stk_t
        (truck-cap ?t - truck)                - stk_t
        (in-truck-sugar ?b - brand ?t - truck) - stk_t
        (capacity ?c - crane)                 - cap_t
        (service-time ?c - crane)             - cap_t
        (max-service-time ?c - crane)         - cap_t
        (cost-process ?m - mill)              - cap_t
        (unharvest-field)                     - chg_t
    )

    ;; Produce one unit of sugar from one unit of raw cane.
    (:action produce_sugar
        :parameters (?r - raw-cane ?m - mill ?b - brand)
        :precondition (and (current-process ?m ?b)
                           (available ?m)
                           (produce ?m ?b)
                           (> (has-resource ?r ?m) 0)
                           (> (max-changing ?m) 0))
        :effect (and (increase (in-storage ?m ?b) 1)
                     (decrease (has-resource ?r ?m) 1)
                     (busy ?m)
                     (not (available ?m)))
    )

    ;; Produce a full batch (max-produce units) of sugar at once.
    (:action produce_sugar_max
        :parameters (?r - raw-cane ?m - mill ?b - brand)
        :precondition (and (current-process ?m ?b)
                           (available ?m)
                           (produce ?m ?b)
                           (>= (has-resource ?r ?m) (max-produce ?m))
                           (> (max-changing ?m) 0))
        :effect (and (increase (in-storage ?m ?b) (max-produce ?m))
                     (decrease (has-resource ?r ?m) (max-produce ?m))
                     (busy ?m)
                     (not (available ?m)))
    )

    ;; Place a resupply order when the mill has run out of cane.
    (:action order-sugar-cane
        :parameters (?r - raw-cane ?m - mill)
        :precondition (= (has-resource ?r ?m) 0)
        :effect (place-order ?r ?m)
    )

    ;; Reset the mill after a production run.
    (:action setting-machine
        :parameters (?m - mill)
        :precondition (busy ?m)
        :effect (and (not (busy ?m)) (available ?m))
    )

    ;; Switch the mill to a different brand (uses one brand-change slot).
    (:action change-product
        :parameters (?m - mill ?b1 ?b2 - brand)
        :precondition (and (current-process ?m ?b1) (change-process ?b1 ?b2))
        :effect (and (current-process ?m ?b2)
                     (not (current-process ?m ?b1))
                     (decrease (max-changing ?m) 1))
    )

    ;; Source raw cane from the farm (consumes one unharvested-field unit).
    (:action sugar-cane-farm
        :parameters (?r - raw-cane ?m - mill)
        :precondition (and (place-order ?r ?m) (> (unharvest-field) 0))
        :effect (and (decrease (unharvest-field) 1)
                     (increase (has-resource ?r ?m) 5)
                     (not (place-order ?r ?m)))
    )

    ;; Transfer raw cane between two mills.
    (:action sugar-cane-mills
        :parameters (?r - raw-cane ?m1 ?m2 - mill)
        :precondition (and (place-order ?r ?m1) (> (has-resource ?r ?m2) 0))
        :effect (and (increase (has-resource ?r ?m1) 1)
                     (decrease (has-resource ?r ?m2) 1)
                     (not (place-order ?r ?m1)))
    )

    ;; Load truck using crane (loads crane-capacity units at once).
    (:action load_truck_crane
        :parameters (?b - brand ?t - truck ?l - location ?c - crane)
        :precondition (and (= (loader-at ?t) ?l)
                           (= (loader-at ?c) ?l)
                           (>= (in-storage ?l ?b) (capacity ?c))
                           (>= (truck-cap ?t) (capacity ?c))
                           (ready-crane ?c))
        :effect (and (decrease (in-storage ?l ?b) (capacity ?c))
                     (decrease (truck-cap ?t) (capacity ?c))
                     (increase (in-truck-sugar ?b ?t) (capacity ?c)))
    )

    ;; Trigger crane maintenance check when service-time reaches 0.
    (:action check-service
        :parameters (?c - crane ?l - location)
        :precondition (and (= (loader-at ?c) ?l)
                           (= (service-time ?c) 0))
        :effect (and (not (ready-crane ?c))
                     (service-crane ?c)
                     (increase (service-time ?c) (max-service-time ?c)))
    )

    ;; Complete crane maintenance; crane is ready again.
    (:action maintainence-crane
        :parameters (?c - crane ?l - location)
        :precondition (and (= (loader-at ?c) ?l) (service-crane ?c))
        :effect (ready-crane ?c)
    )

    ;; Load one unit of sugar manually (no crane).
    (:action load-truck-manual
        :parameters (?b - brand ?t - truck ?l - location)
        :precondition (and (= (loader-at ?t) ?l)
                           (> (in-storage ?l ?b) 0)
                           (> (truck-cap ?t) 0))
        :effect (and (decrease (in-storage ?l ?b) 1)
                     (decrease (truck-cap ?t) 1)
                     (increase (in-truck-sugar ?b ?t) 1))
    )

    ;; Drive the truck to a connected location.
    (:action drive_truck
        :parameters (?t - truck ?y1 ?y2 - location)
        :precondition (and (= (loader-at ?t) ?y1)
                           (member ?y2 (connects ?y1)))
        :effect (assign (loader-at ?t) ?y2)
    )

    ;; Unload one unit of sugar from truck to location.
    (:action unload_truck
        :parameters (?b - brand ?t - truck ?l - location)
        :precondition (and (= (loader-at ?t) ?l)
                           (> (in-truck-sugar ?b ?t) 0))
        :effect (and (increase (in-storage ?l ?b) 1)
                     (decrease (in-truck-sugar ?b ?t) 1)
                     (increase (truck-cap ?t) 1))
    )
)