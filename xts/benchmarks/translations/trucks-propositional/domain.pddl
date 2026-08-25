;; PDDL-XTS translation of pddl/test/trucks-propositional (IPC-5).
;; The original encodes time as a TYPE with next/le CHAINS:
;;   - 7 time objects t0..t6
;;   - (time-now ?t) current-time proposition
;;   - (next ?t1 ?t2) successor chain (6 predicates)
;;   - (le ?t1 ?t2) ≤ relation (21 predicates)
;;   - drive advances (time-now) via next; deliver checks (le t_now t_deadline)
;;
;; XTS replaces this entire machinery with two bounded-int fluents:
;;   (current-time) - time_t   where time_t = (number 0 6)
;;       drive increments by 1; IPAR prunes drive at time 6 (would exceed max)
;;   (deadline ?p) - time_t    per-package static deadline
;;       deliver precondition: (<= (current-time) (deadline ?p))
;;       goal reverts to (at-destination ?p ?l) — deadline enforced by deliver
;;
;; Additional translations:
;;   (at ?truck ?l)       -> object fluent (truck-at ?t) - location  [total function]
;;   (at ?package ?l)     -> stays boolean  [partial: pkg either at-loc or in-truck]
;;   (delivered ?p ?l ?t) -> dropped (replaced by at-destination + deadline check)
;;
;; (connected ?l1 ?l2) and (closer ?a1 ?a2) stay boolean predicates.
;; Note: (:sets) + forall-imply in preconditions triggers a C++ backend segfault
;; (mixing set features with quantified preconditions; same ADL regression as
;; openstacks/logistics with extra set types). Boolean predicates work correctly.
;;
;; forall/imply in load/unload preserved as ADL (QR compiler expands object-typed
;; forall before C++ backend; uses default --up-compilers IPAR,QR,GR).

(define (domain trucks-xts)
(:requirements :typing :bounded-integers :object-fluents)
(:types truckarea location locatable - object
        truck package - locatable
        time_t - (number 0 6))
(:predicates
    (at ?p - package ?l - location)
    (in ?p - package ?t - truck ?a - truckarea)
    (free ?a - truckarea ?t - truck)
    (at-destination ?p - package ?l - location)
    (closer ?a1 - truckarea ?a2 - truckarea)
    (connected ?l1 ?l2 - location))
(:functions
    (truck-at ?t - truck) - location
    (current-time)        - time_t
    (deadline ?p - package) - time_t)

(:action load
    :parameters (?p - package ?t - truck ?a1 - truckarea ?l - location)
    :precondition (and (= (truck-at ?t) ?l)
                       (at ?p ?l)
                       (free ?a1 ?t)
                       (forall (?a2 - truckarea)
                               (imply (closer ?a2 ?a1) (free ?a2 ?t))))
    :effect (and (not (at ?p ?l))
                 (not (free ?a1 ?t))
                 (in ?p ?t ?a1)))

(:action unload
    :parameters (?p - package ?t - truck ?a1 - truckarea ?l - location)
    :precondition (and (= (truck-at ?t) ?l)
                       (in ?p ?t ?a1)
                       (forall (?a2 - truckarea)
                               (imply (closer ?a2 ?a1) (free ?a2 ?t))))
    :effect (and (not (in ?p ?t ?a1))
                 (free ?a1 ?t)
                 (at ?p ?l)))

(:action drive
    :parameters (?t - truck ?from ?to - location)
    :precondition (and (= (truck-at ?t) ?from)
                       (connected ?from ?to))
    :effect (and (assign (truck-at ?t) ?to)
                 (increase (current-time) 1)))

(:action deliver
    :parameters (?p - package ?l - location)
    :precondition (and (at ?p ?l)
                       (<= (current-time) (deadline ?p)))
    :effect (and (not (at ?p ?l))
                 (at-destination ?p ?l)))
)
