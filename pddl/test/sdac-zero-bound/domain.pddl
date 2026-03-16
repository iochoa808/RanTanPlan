;; SDAC domain where a cost expression has a zero lower bound.
;; The counter starts at 0, so (value ?c) can be 0 at the RPG fixpoint,
;; giving the increment action a cost lower bound of 0.

(define (domain sdac-zero-bound)
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
