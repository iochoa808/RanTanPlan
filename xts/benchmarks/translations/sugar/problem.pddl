;; PDDL-XTS translation of pddl/test/sugar/problem.pddl (simple-supply-chain).
;; (at-location truck1/crane1 mill1) -> (= (loader-at truck1/crane1) mill1).
;; (connected mill1 depot1) + (connected depot1 mill1) -> symmetric connects sets.
;; Metric accumulators (mill-cost, inventory-cost, handling-cost) are dropped.

(define (problem simple-supply-chain-xts)
    (:domain supply-chain-xts)
    (:objects
        brand1 brand2 - brand
        sugar-cane    - raw-cane
        truck1        - truck
        depot1        - depot
        mill1         - mill
        crane1        - crane
    )
    (:init
        ;; Connectivity
        (= (connects mill1)  (set.mk (depot1)))
        (= (connects depot1) (set.mk (mill1)))

        ;; Loader locations
        (= (loader-at truck1) mill1)
        (= (loader-at crane1) mill1)

        ;; Mill state
        (available mill1)
        (produce mill1 brand1)
        (produce mill1 brand2)
        (current-process mill1 brand1)
        (change-process brand1 brand2)
        (change-process brand2 brand1)
        (= (cost-process mill1)  1)
        (= (max-produce mill1)   3)
        (= (max-changing mill1)  2)
        (= (has-resource sugar-cane mill1) 5)

        ;; Storage at locations (all zero initially)
        (= (in-storage mill1  brand1) 0)
        (= (in-storage mill1  brand2) 0)
        (= (in-storage depot1 brand1) 0)
        (= (in-storage depot1 brand2) 0)

        ;; Truck
        (= (truck-cap truck1)           5)
        (= (in-truck-sugar brand1 truck1) 0)
        (= (in-truck-sugar brand2 truck1) 0)

        ;; Crane
        (ready-crane crane1)
        (= (capacity crane1)       2)
        (= (service-time crane1)   5)
        (= (max-service-time crane1) 5)

        ;; Farm
        (= (unharvest-field) 2)
    )
    (:goal (>= (in-storage depot1 brand1) 2))
)