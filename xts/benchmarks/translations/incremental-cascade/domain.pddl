;; PDDL-XTS translation of pddl/test/incremental-cascade.
;; FEATURE: bounded integers.
;;   (x)/(y) -> (number 0 100).  (This is a C++ ActionPruningPass test harness
;;   domain; the translation just rebinds its numeric fluents to bounded ints —
;;   the goal y>=50 is reached via direct-set.)

(define (domain incremental-cascade-xts)
    (:requirements :strips :numeric-fluents :bounded-integers)
    (:types val - (number 0 100))
    (:predicates (g))
    (:functions (x) - val (y) - val)

    (:action waste :parameters () :effect (assign (x) 100))
    (:action bridge :parameters () :precondition (g) :effect (assign (y) (x)))
    (:action trigger :parameters () :effect (g))
    (:action direct-set :parameters () :effect (assign (y) 50))
)
