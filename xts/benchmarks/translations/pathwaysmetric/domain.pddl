;; PDDL-XTS translation of pddl/test/pathwaysmetric.
;; FEATURE: bounded integers.
;;   (available ?x), need-for-*, prod-by-* and num-subs become bounded ints
;;   (number 0 3).  The real-valued (duration-*) fluents are dropped — they only
;;   feed the temporal/metric objective, which the a-temporal satisficing solve
;;   ignores.  Reaction relations stay boolean.

(define (domain pathways-metric-xts)
    (:requirements :typing :numeric-fluents :bounded-integers)
    (:types
        molecule - object
        simple complex - molecule
        qty - (number 0 3)
    )
    (:predicates
        (association-reaction ?x1 ?x2 - molecule ?x3 - complex)
        (catalyzed-association-reaction ?x1 ?x2 - molecule ?x3 - complex)
        (catalyzed-self-association-reaction ?x1 - molecule ?x3 - complex)
        (synthesis-reaction ?x1 ?x2 - molecule)
        (possible ?x - molecule)
        (chosen ?x - simple)
    )
    (:functions
        (available ?x - molecule) - qty
        (need-for-association ?x1 ?x2 - molecule ?x3 - complex) - qty
        (need-for-catalyzed-association ?x1 ?x2 - molecule ?x3 - complex) - qty
        (need-for-catalyzed-self-association ?x1 - molecule ?x3 - complex) - qty
        (need-for-synthesis ?x1 ?x2 - molecule) - qty
        (prod-by-association ?x1 ?x2 - molecule ?x3 - complex) - qty
        (prod-by-catalyzed-association ?x1 ?x2 - molecule ?x3 - complex) - qty
        (prod-by-catalyzed-self-association ?x1 - molecule ?x3 - complex) - qty
        (prod-by-synthesis ?x1 ?x2 - molecule) - qty
        (num-subs) - qty
    )

    (:action choose
        :parameters (?x - simple)
        :precondition (and (possible ?x) (< (num-subs) 3))
        :effect (and (chosen ?x) (not (possible ?x)) (increase (num-subs) 1))
    )
    (:action initialize
        :parameters (?x - simple)
        :precondition (and (chosen ?x) (< (available ?x) 3))
        :effect (increase (available ?x) 1)
    )
    (:action associate
        :parameters (?x1 ?x2 - molecule ?x3 - complex)
        :precondition (and (>= (available ?x1) (need-for-association ?x1 ?x2 ?x3))
                           (>= (available ?x2) (need-for-association ?x2 ?x1 ?x3))
                           (association-reaction ?x1 ?x2 ?x3) (< (available ?x3) 3))
        :effect (and (decrease (available ?x1) (need-for-association ?x1 ?x2 ?x3))
                     (decrease (available ?x2) (need-for-association ?x2 ?x1 ?x3))
                     (increase (available ?x3) (prod-by-association ?x1 ?x2 ?x3)))
    )
    (:action associate-with-catalyze
        :parameters (?x1 ?x2 - molecule ?x3 - complex)
        :precondition (and (>= (available ?x1) (need-for-catalyzed-association ?x1 ?x2 ?x3))
                           (>= (available ?x2) (need-for-catalyzed-association ?x2 ?x1 ?x3))
                           (catalyzed-association-reaction ?x1 ?x2 ?x3) (< (available ?x3) 3))
        :effect (and (decrease (available ?x1) (need-for-catalyzed-association ?x1 ?x2 ?x3))
                     (increase (available ?x3) (prod-by-catalyzed-association ?x1 ?x2 ?x3)))
    )
    (:action self-associate-with-catalyze
        :parameters (?x1 - molecule ?x3 - complex)
        :precondition (and (>= (available ?x1) (need-for-catalyzed-self-association ?x1 ?x3))
                           (catalyzed-self-association-reaction ?x1 ?x3) (< (available ?x3) 3))
        :effect (and (decrease (available ?x1) (need-for-catalyzed-self-association ?x1 ?x3))
                     (increase (available ?x3) (prod-by-catalyzed-self-association ?x1 ?x3)))
    )
    (:action synthesize
        :parameters (?x1 ?x2 - molecule)
        :precondition (and (>= (available ?x1) (need-for-synthesis ?x1 ?x2))
                           (synthesis-reaction ?x1 ?x2) (< (available ?x2) 3))
        :effect (increase (available ?x2) (prod-by-synthesis ?x1 ?x2))
    )
)
