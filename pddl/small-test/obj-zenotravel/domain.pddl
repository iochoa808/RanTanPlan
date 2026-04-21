;; Zenotravel with object fluents.
;;
;; The original domain uses two boolean predicates to encode position:
;;   (located ?x - locatable ?c - city)   -- at a city
;;   (in ?p - person ?a - aircraft)        -- aboard a plane
;;
;; These form a cross-predicate exactly-one group per entity: a person
;; is always at exactly one city OR aboard exactly one aircraft.
;; The boolean encoding forces the planner to discover this invariant.
;;
;; This reformulation uses object fluents:
;;   (aircraft-at ?a - aircraft) - city       -- planes are always at a city
;;   (person-at ?p - person) - locale         -- persons are at a city or aboard a plane
;;
;; where `locale` is a supertype of `city` and `aircraft`.
;; The exactly-one constraint is structurally guaranteed: a function
;; has exactly one value. No mutex discovery needed.

(define (domain obj-zenotravel)
(:requirements :typing :numeric-fluents :object-fluents)
(:types
    locale - object
    city aircraft - locale   ;; aircraft are locales (persons can be aboard them)
    person - object
)

(:functions
    ;; Object fluents: position as a single-valued function.
    ;; aircraft-at returns a city (planes fly between cities).
    ;; person-at returns a locale (persons can be at a city OR aboard an aircraft).
    (aircraft-at ?a - aircraft) - city
    (person-at ?p - person) - locale

    ;; Numeric fluents (unchanged from original)
    (fuel ?a - aircraft)
    (distance ?c1 - city ?c2 - city)
    (slow-burn ?a - aircraft)
    (fast-burn ?a - aircraft)
    (capacity ?a - aircraft)
    (total-fuel-used)
    (onboard ?a - aircraft)
    (zoom-limit ?a - aircraft)
)

(:action board
 :parameters (?p - person ?a - aircraft ?c - city)
 :precondition (and (= (person-at ?p) ?c)
                    (= (aircraft-at ?a) ?c))
 :effect (and (assign (person-at ?p) ?a)
              (increase (onboard ?a) 1)))

(:action debark
 :parameters (?p - person ?a - aircraft ?c - city)
 :precondition (and (= (person-at ?p) ?a)
                    (= (aircraft-at ?a) ?c))
 :effect (and (assign (person-at ?p) ?c)
              (decrease (onboard ?a) 1)))

(:action fly-slow
 :parameters (?a - aircraft ?c1 ?c2 - city)
 :precondition (and (= (aircraft-at ?a) ?c1)
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
 :precondition (> (capacity ?a) (fuel ?a))
 :effect (assign (fuel ?a) (capacity ?a)))
)
