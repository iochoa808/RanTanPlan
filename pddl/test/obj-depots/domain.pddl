;; Hybrid object-fluent version of Depots.
;; Object fluents for total relationships (truck/hoist always at a place).
;; Boolean predicates for partial relationships (crate stacking, holding).

(define (domain Depot-object-fluents)
  (:requirements :typing :equality :numeric-fluents :object-fluents)
  (:types place locatable - object
	  depot distributor - place
	  truck hoist surface - locatable
	  pallet crate - surface)

  (:predicates
   (clear ?s - surface)
   (on ?c - crate ?s - surface)
   (in-truck ?c - crate ?t - truck)
   (holding ?h - hoist ?c - crate)
   (available ?h - hoist))

  (:functions
   (load-limit ?t - truck)
   (current-load ?t - truck)
   (weight ?c - crate)
   (fuel-cost)
   (position-of ?l - locatable) - place)


  (:action drive
   :parameters (?t - truck ?p - place)
   :effect (and (assign (position-of ?t) ?p)
		(increase (fuel-cost) 10)))

  (:action lift
   :parameters (?h - hoist ?c - crate ?s - surface)
   :precondition (and (= (position-of ?h) (position-of ?c))
		      (available ?h)
		      (clear ?c)
		      (on ?c ?s))
   :effect (and (holding ?h ?c)
                (not (available ?h))
		(not (on ?c ?s))
		(not (clear ?c))
		(clear ?s)
		(increase (fuel-cost) 1)))

  (:action drop
   :parameters (?h - hoist ?c - crate ?s - surface)
   :precondition (and (= (position-of ?h) (position-of ?s))
                      (holding ?h ?c)
		      (clear ?s))
   :effect (and (not (holding ?h ?c))
                (available ?h)
                (assign (position-of ?c) (position-of ?h))
		(on ?c ?s)
		(clear ?c)
		(not (clear ?s))))

  (:action load
   :parameters (?h - hoist ?c - crate ?t - truck)
   :precondition (and (= (position-of ?h) (position-of ?t))
		      (holding ?h ?c)
		      (<= (+ (current-load ?t) (weight ?c))
			  (load-limit ?t)))
   :effect (and (not (holding ?h ?c))
                (available ?h)
		(in-truck ?c ?t)
		(increase (current-load ?t) (weight ?c))))

  (:action unload
   :parameters (?h - hoist ?c - crate ?t - truck)
   :precondition (and (= (position-of ?h) (position-of ?t))
		      (available ?h)
		      (in-truck ?c ?t))
   :effect (and (not (in-truck ?c ?t))
		(holding ?h ?c)
		(not (available ?h))
		(decrease (current-load ?t) (weight ?c))))
  )
