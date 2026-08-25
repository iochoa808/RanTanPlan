;; PDDL-XTS translation of pddl/test/zenotravel.
;; Extends obj-zenotravel (pddl/test/obj-zenotravel) with one bounded integer:
;;   (onboard ?a) - obd_t  where obd_t = (number 0 8)
;;       zoom-limit = 8 in all known instances; the type bounds the max passenger
;;       count that could ever fit, while the fly-fast precondition
;;       (<= (onboard ?a) (zoom-limit ?a)) still enforces the per-aircraft limit.
;;
;; All other numeric fluents stay as plain numeric (:numeric-fluents) because
;; they have ranges that are impractical to bound:
;;   fuel / capacity     : up to 6000 in this instance
;;   distance            : 678–810 (static; folded by StaticFluentPass)
;;   slow-burn/fast-burn : 4 / 15  (static; folded)
;;   zoom-limit          : 8       (static; folded)
;;   total-fuel-used     : dropped — metric accumulator; not needed for satisficing
;;
;; Object fluent (from obj-zenotravel): (aircraft-at ?a) - city
;; Person location stays boolean: (at ?p ?c) is partial (persons can be in-plane).
;; Mixed requirements: :numeric-fluents for plain numerics, :bounded-integers for obd_t.

(define (domain zenotravel-xts)
(:requirements :typing :equality :numeric-fluents :bounded-integers :object-fluents)
(:types aircraft person city - object
        obd_t - (number 0 8))
(:predicates
    (at ?p - person ?c - city)
    (in ?p - person ?a - aircraft))
(:functions
    (aircraft-at ?a - aircraft)  - city
    (fuel ?a - aircraft)
    (distance ?c1 - city ?c2 - city)
    (slow-burn ?a - aircraft)
    (fast-burn ?a - aircraft)
    (capacity ?a - aircraft)
    (onboard ?a - aircraft)      - obd_t
    (zoom-limit ?a - aircraft))

(:action board
    :parameters (?p - person ?a - aircraft ?c - city)
    :precondition (and (at ?p ?c)
                       (= (aircraft-at ?a) ?c))
    :effect (and (not (at ?p ?c))
                 (in ?p ?a)
                 (increase (onboard ?a) 1)))

(:action debark
    :parameters (?p - person ?a - aircraft ?c - city)
    :precondition (and (in ?p ?a)
                       (= (aircraft-at ?a) ?c))
    :effect (and (not (in ?p ?a))
                 (at ?p ?c)
                 (decrease (onboard ?a) 1)))

(:action fly-slow
    :parameters (?a - aircraft ?c1 ?c2 - city)
    :precondition (and (= (aircraft-at ?a) ?c1)
                       (not (= ?c1 ?c2))
                       (>= (fuel ?a)
                           (* (distance ?c1 ?c2) (slow-burn ?a))))
    :effect (and (assign (aircraft-at ?a) ?c2)
                 (decrease (fuel ?a)
                           (* (distance ?c1 ?c2) (slow-burn ?a)))))

(:action fly-fast
    :parameters (?a - aircraft ?c1 ?c2 - city)
    :precondition (and (= (aircraft-at ?a) ?c1)
                       (not (= ?c1 ?c2))
                       (>= (fuel ?a)
                           (* (distance ?c1 ?c2) (fast-burn ?a)))
                       (<= (onboard ?a) (zoom-limit ?a)))
    :effect (and (assign (aircraft-at ?a) ?c2)
                 (decrease (fuel ?a)
                           (* (distance ?c1 ?c2) (fast-burn ?a)))))

(:action refuel
    :parameters (?a - aircraft)
    :precondition (> (capacity ?a) (fuel ?a))
    :effect (assign (fuel ?a) (capacity ?a)))
)
