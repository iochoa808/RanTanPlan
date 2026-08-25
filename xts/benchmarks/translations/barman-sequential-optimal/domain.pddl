;; PDDL-XTS translation of pddl/test/barman-sequential-optimal.
;; FEATURE: bounded integers.
;;   The shaker fill level (shaker-level ?s ?l) + (next ?l1 ?l2) chain becomes a
;;   bounded-int gauge (shaker-level ?s) : (number 0 2).  Pouring a shot in
;;   increments it, pouring the shaker out decrements it, emptying resets to 0
;;   (the old shaker-empty-level was l0 = 0).  All container/hand state stays
;;   boolean.  Cost metric dropped (satisficing).

(define (domain barman-xts)
    (:requirements :strips :typing :bounded-integers)
    (:types
        hand beverage dispenser container - object
        ingredient cocktail - beverage
        shot shaker - container
        lvl - (number 0 2)
    )
    (:predicates
        (ontable ?c - container)
        (holding ?h - hand ?c - container)
        (handempty ?h - hand)
        (empty ?c - container)
        (contains ?c - container ?b - beverage)
        (clean ?c - container)
        (used ?c - container ?b - beverage)
        (dispenses ?d - dispenser ?i - ingredient)
        (unshaked ?s - shaker)
        (shaked ?s - shaker)
        (cocktail-part1 ?c - cocktail ?i - ingredient)
        (cocktail-part2 ?c - cocktail ?i - ingredient)
    )
    (:functions
        (shaker-level ?s - shaker) - lvl
    )

    (:action grasp
        :parameters (?h - hand ?c - container)
        :precondition (and (ontable ?c) (handempty ?h))
        :effect (and (not (ontable ?c)) (not (handempty ?h)) (holding ?h ?c))
    )
    (:action leave
        :parameters (?h - hand ?c - container)
        :precondition (holding ?h ?c)
        :effect (and (not (holding ?h ?c)) (handempty ?h) (ontable ?c))
    )
    (:action fill-shot
        :parameters (?s - shot ?i - ingredient ?h1 ?h2 - hand ?d - dispenser)
        :precondition (and (holding ?h1 ?s) (handempty ?h2) (dispenses ?d ?i)
                           (empty ?s) (clean ?s))
        :effect (and (not (empty ?s)) (contains ?s ?i) (not (clean ?s)) (used ?s ?i))
    )
    (:action refill-shot
        :parameters (?s - shot ?i - ingredient ?h1 ?h2 - hand ?d - dispenser)
        :precondition (and (holding ?h1 ?s) (handempty ?h2) (dispenses ?d ?i)
                           (empty ?s) (used ?s ?i))
        :effect (and (not (empty ?s)) (contains ?s ?i))
    )
    (:action empty-shot
        :parameters (?h - hand ?p - shot ?b - beverage)
        :precondition (and (holding ?h ?p) (contains ?p ?b))
        :effect (and (not (contains ?p ?b)) (empty ?p))
    )
    (:action clean-shot
        :parameters (?s - shot ?b - beverage ?h1 ?h2 - hand)
        :precondition (and (holding ?h1 ?s) (handempty ?h2) (empty ?s) (used ?s ?b))
        :effect (and (not (used ?s ?b)) (clean ?s))
    )
    (:action pour-shot-to-clean-shaker
        :parameters (?s - shot ?i - ingredient ?d - shaker ?h1 - hand)
        :precondition (and (holding ?h1 ?s) (contains ?s ?i) (empty ?d) (clean ?d)
                           (< (shaker-level ?d) 2))
        :effect (and (not (contains ?s ?i)) (empty ?s) (contains ?d ?i) (not (empty ?d))
                     (not (clean ?d)) (unshaked ?d) (increase (shaker-level ?d) 1))
    )
    (:action pour-shot-to-used-shaker
        :parameters (?s - shot ?i - ingredient ?d - shaker ?h1 - hand)
        :precondition (and (holding ?h1 ?s) (contains ?s ?i) (unshaked ?d)
                           (< (shaker-level ?d) 2))
        :effect (and (not (contains ?s ?i)) (contains ?d ?i) (empty ?s)
                     (increase (shaker-level ?d) 1))
    )
    (:action empty-shaker
        :parameters (?h - hand ?s - shaker ?b - cocktail)
        :precondition (and (holding ?h ?s) (contains ?s ?b) (shaked ?s))
        :effect (and (not (shaked ?s)) (assign (shaker-level ?s) 0)
                     (not (contains ?s ?b)) (empty ?s))
    )
    (:action clean-shaker
        :parameters (?h1 ?h2 - hand ?s - shaker)
        :precondition (and (holding ?h1 ?s) (handempty ?h2) (empty ?s))
        :effect (clean ?s)
    )
    (:action shake
        :parameters (?b - cocktail ?d1 ?d2 - ingredient ?s - shaker ?h1 ?h2 - hand)
        :precondition (and (holding ?h1 ?s) (handempty ?h2) (contains ?s ?d1)
                           (contains ?s ?d2) (cocktail-part1 ?b ?d1) (cocktail-part2 ?b ?d2)
                           (unshaked ?s))
        :effect (and (not (unshaked ?s)) (not (contains ?s ?d1)) (not (contains ?s ?d2))
                     (shaked ?s) (contains ?s ?b))
    )
    (:action pour-shaker-to-shot
        :parameters (?b - beverage ?d - shot ?h - hand ?s - shaker)
        :precondition (and (holding ?h ?s) (shaked ?s) (empty ?d) (clean ?d)
                           (contains ?s ?b) (>= (shaker-level ?s) 1))
        :effect (and (not (clean ?d)) (not (empty ?d)) (contains ?d ?b)
                     (decrease (shaker-level ?s) 1))
    )
)
