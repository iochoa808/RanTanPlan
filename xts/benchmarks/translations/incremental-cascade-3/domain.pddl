;; PDDL-XTS translation of pddl/test/incremental-cascade-3.
;; FEATURE: bounded integers. (x)/(y)/(z)/(h) -> (number 0 100).
;; C++ ActionPruningPass test-harness domain; goal reached via direct-y/direct-z.

(define (domain incremental-cascade-3-xts)
    (:requirements :strips :numeric-fluents :bounded-integers)
    (:types val - (number 0 100))
    (:predicates (g) (g2) (done))
    (:functions (x) - val (y) - val (z) - val (h) - val)

    (:action waste :parameters () :effect (assign (x) 100))
    (:action bridge :parameters () :precondition (g) :effect (assign (y) (x)))
    (:action trigger :parameters () :effect (and (g) (assign (h) 100)))
    (:action bridge2 :parameters () :precondition (g2) :effect (assign (z) (h)))
    (:action trigger2 :parameters () :effect (g2))
    (:action direct-y :parameters () :effect (assign (y) 50))
    (:action direct-z :parameters () :effect (assign (z) 50))
    (:action finish :parameters () :precondition (and (>= (y) 50) (>= (z) 50)) :effect (done))
)
