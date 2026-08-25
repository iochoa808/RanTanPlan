;; PDDL-XTS translation of pddl/test/pathways-propositional.
;; FEATURE: bounded integers.
;;   The (num-subs ?l) + (next ?l1 ?l2) "substances chosen" level chain becomes a
;;   bounded-int budget (num-subs) : (number 0 2); choose increments it (guard < 2).
;;   Reaction relations and availability stay boolean.

(define (domain pathways-prop-xts)
    (:requirements :typing :adl :bounded-integers)
    (:types
        molecule - object
        simple complex - molecule
        budget - (number 0 2)
    )
    (:constants pCAF-p300 pRbp1p2-AP2 - complex)
    (:predicates
        (association-reaction ?x1 ?x2 - molecule ?x3 - complex)
        (catalyzed-association-reaction ?x1 ?x2 - molecule ?x3 - complex)
        (synthesis-reaction ?x1 ?x2 - molecule)
        (possible ?x - molecule)
        (available ?x - molecule)
        (chosen ?s - simple)
        (goal1)
    )
    (:functions
        (num-subs) - budget
    )

    (:action choose
        :parameters (?x - simple)
        :precondition (and (possible ?x) (not (chosen ?x)) (< (num-subs) 2))
        :effect (and (chosen ?x) (increase (num-subs) 1))
    )
    (:action initialize
        :parameters (?x - simple)
        :precondition (chosen ?x)
        :effect (available ?x)
    )
    (:action associate
        :parameters (?x1 ?x2 - molecule ?x3 - complex)
        :precondition (and (association-reaction ?x1 ?x2 ?x3) (available ?x1) (available ?x2))
        :effect (and (not (available ?x1)) (not (available ?x2)) (available ?x3))
    )
    (:action associate-with-catalyze
        :parameters (?x1 ?x2 - molecule ?x3 - complex)
        :precondition (and (catalyzed-association-reaction ?x1 ?x2 ?x3) (available ?x1) (available ?x2))
        :effect (and (not (available ?x1)) (available ?x3))
    )
    (:action synthesize
        :parameters (?x1 ?x2 - molecule)
        :precondition (and (synthesis-reaction ?x1 ?x2) (available ?x1))
        :effect (available ?x2)
    )
    (:action DUMMY-ACTION-1
        :parameters ()
        :precondition (or (available pRbp1p2-AP2) (available pCAF-p300))
        :effect (goal1)
    )
)
