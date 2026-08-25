;; PDDL-XTS translation of pddl/test/trucks-propositional/problem.pddl (truck-1).
;; Time objects t0..t6 removed; replaced by (= (current-time) 0).
;; 6 (next tX tY) predicates and 21 (le tX tY) predicates removed entirely.
;; (connected ?l1 ?l2) stays boolean (sets + forall-imply in precond = backend segfault).
;; (closer a1 a2) stays as boolean predicate.
;; (at truck1 l3) -> (= (truck-at truck1) l3); package at predicates stay boolean.
;; Per-package deadlines:
;;   package1 goal (delivered package1 l3 t3) -> (deadline package1) = 3
;;   package2 goal (at-destination package2 l1) -> no deadline, (deadline package2) = 6
;;   package3 goal (delivered package3 l1 t6)  -> (deadline package3) = 6
;; Goal simplified: (at-destination ?p ?l) for all; deadline enforced by deliver action.

(define (problem truck-1-xts) (:domain trucks-xts)
(:objects
    truck1   - truck
    package1 package2 package3 - package
    l1 l2 l3 - location
    a1 a2    - truckarea)
(:init
    ;; Truck position (object fluent — always at exactly one location)
    (= (truck-at truck1) l3)

    ;; Truck areas: both free initially
    (free a1 truck1)
    (free a2 truck1)

    ;; Packages start at l2
    (at package1 l2)
    (at package2 l2)
    (at package3 l2)

    ;; Road connectivity: stays boolean (sets + forall-imply = backend segfault)
    (connected l1 l2) (connected l1 l3)
    (connected l2 l1) (connected l2 l3)
    (connected l3 l1) (connected l3 l2)

    ;; LIFO truck-area ordering: a1 is closer to front than a2 (stays boolean).
    (closer a1 a2)

    ;; Time: current time starts at 0 (was time-now t0).
    (= (current-time) 0)

    ;; Per-package deadlines (static; checked by deliver precondition).
    ;; package1: must be delivered by t3 (time step 3)
    ;; package2: no deadline constraint -> use max (6)
    ;; package3: must be delivered by t6 (time step 6)
    (= (deadline package1) 3)
    (= (deadline package2) 6)
    (= (deadline package3) 6))

(:goal (and
    (at-destination package1 l3)
    (at-destination package2 l1)
    (at-destination package3 l1)))
)
