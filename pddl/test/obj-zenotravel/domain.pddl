;; Object-fluent version of Zenotravel.
;; Replaces predicates (located ?x ?c) and (in ?p ?a) with
;; object fluents (location ?x) and (aboard ?p).

(define (domain obj-zenotravel)
(:requirements :typing :numeric-fluents :object-fluents)
(:types locatable city - object
	aircraft person - locatable)
(:constants nowhere - city
            no-aircraft - aircraft)
(:functions (location ?x - locatable) - city
            (aboard ?p - person) - aircraft
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
 :precondition (and (= (location ?p) ?c)
                 (= (location ?a) ?c))
 :effect (and (assign (location ?p) nowhere)
              (assign (aboard ?p) ?a)
		(increase (onboard ?a) 1)))


(:action debark
 :parameters (?p - person ?a - aircraft ?c - city)
 :precondition (and (= (aboard ?p) ?a)
                 (= (location ?a) ?c))
 :effect (and (assign (aboard ?p) no-aircraft)
              (assign (location ?p) ?c)
		(decrease (onboard ?a) 1)))

(:action fly-slow
 :parameters (?a - aircraft ?c1 ?c2 - city)
 :precondition (and (= (location ?a) ?c1)
                 (>= (fuel ?a)
                         (* (distance ?c1 ?c2) (slow-burn ?a))))
 :effect (and (assign (location ?a) ?c2)
              (increase (total-fuel-used)
                         (* (distance ?c1 ?c2) (slow-burn ?a)))
              (decrease (fuel ?a)
                         (* (distance ?c1 ?c2) (slow-burn ?a)))))

(:action fly-fast
 :parameters (?a - aircraft ?c1 ?c2 - city)
 :precondition (and (= (location ?a) ?c1)
                 (>= (fuel ?a)
                         (* (distance ?c1 ?c2) (fast-burn ?a)))
                 (<= (onboard ?a) (zoom-limit ?a)))
 :effect (and (assign (location ?a) ?c2)
              (increase (total-fuel-used)
                         (* (distance ?c1 ?c2) (fast-burn ?a)))
              (decrease (fuel ?a)
                         (* (distance ?c1 ?c2) (fast-burn ?a)))
	)
)

(:action refuel
 :parameters (?a - aircraft)
 :precondition (and (> (capacity ?a) (fuel ?a)))
 :effect (and (assign (fuel ?a) (capacity ?a)))
)

)
