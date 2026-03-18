;; Hybrid object-fluent version of Zenotravel.
;; Uses an object fluent for aircraft location (always at exactly one city),
;; and boolean predicates for person locations and boarding (partial relations).

(define (domain obj-zenotravel)
(:requirements :typing :numeric-fluents :object-fluents)
(:types aircraft person city - object)
(:predicates (at ?p - person ?c - city)
             (in ?p - person ?a - aircraft))
(:functions (aircraft-at ?a - aircraft) - city
            (fuel ?a - aircraft)
            (distance ?c1 - city ?c2 - city)
            (slow-burn ?a - aircraft)
            (fast-burn ?a - aircraft)
            (capacity ?a - aircraft)
            (total-fuel-used)
	    (onboard ?a - aircraft)
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
              (increase (total-fuel-used)
                         (* (distance ?c1 ?c2) (slow-burn ?a)))
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
              (increase (total-fuel-used)
                         (* (distance ?c1 ?c2) (fast-burn ?a)))
              (decrease (fuel ?a)
                         (* (distance ?c1 ?c2) (fast-burn ?a)))))

(:action refuel
 :parameters (?a - aircraft)
 :precondition (and (> (capacity ?a) (fuel ?a)))
 :effect (and (assign (fuel ?a) (capacity ?a))))

)
