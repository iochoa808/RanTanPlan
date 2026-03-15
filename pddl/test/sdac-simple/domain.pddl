;; Simple domain with state-dependent action costs (SDAC).
;; One counter that can be incremented; cost of incrementing = current value.
;; Goal: reach target value and mark done.
;; With value starting at 1 and target at 3:
;;   Optimal: increment(1) + increment(2) + finish(1) = cost 4.

(define (domain sdac-simple)
  (:requirements :numeric-fluents :action-costs :typing)
  (:types counter)
  (:predicates (done) (is-target ?c - counter))
  (:functions
    (value ?c - counter)
    (total-cost)
  )

  (:action increment
    :parameters (?c - counter)
    :precondition (and (<= (value ?c) 10))
    :effect (and (increase (value ?c) 1)
                 (increase (total-cost) (value ?c)))
  )

  (:action finish
    :parameters (?c - counter)
    :precondition (and (is-target ?c) (>= (value ?c) 3))
    :effect (and (done) (increase (total-cost) 1))
  )
)
