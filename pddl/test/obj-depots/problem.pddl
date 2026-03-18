(define (problem depotprob1818)
  (:domain Depot-object-fluents)
  (:objects depot0 - depot
	    distributor0 distributor1 - distributor
	    truck0 truck1 - truck
	    pallet0 pallet1 pallet2 - pallet
	    crate0 crate1 - crate
	    hoist0 hoist1 hoist2 - hoist)
  (:init
   (= (position-of pallet0) depot0)
   (= (position-of pallet1) distributor0)
   (= (position-of pallet2) distributor1)
   (= (position-of truck0) distributor1)
   (= (position-of truck1) depot0)
   (= (position-of hoist0) depot0)
   (= (position-of hoist1) distributor0)
   (= (position-of hoist2) distributor1)
   (= (position-of crate0) distributor0)
   (= (position-of crate1) depot0)
   (on crate0 pallet1)
   (on crate1 pallet0)
   (clear crate0)
   (clear crate1)
   (clear pallet2)
   (available hoist0)
   (available hoist1)
   (available hoist2)
   (= (current-load truck0) 0)
   (= (load-limit truck0) 323)
   (= (current-load truck1) 0)
   (= (load-limit truck1) 220)
   (= (weight crate0) 11)
   (= (weight crate1) 86)
   (= (fuel-cost) 0))

  (:goal (and (on crate0 pallet2)
	      (on crate1 pallet1)))

  (:metric minimize (fuel-cost))
  )
