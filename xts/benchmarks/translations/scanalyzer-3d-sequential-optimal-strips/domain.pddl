;; PDDL-XTS translation of pddl/test/scanalyzer-3d-sequential-optimal-strips.
;; FEATURE: object fluents.
;;   Each car sits on exactly one segment -> object fluent (seg-of ?c) - segment.
;;   analyze-2 / rotate-2 swap two cars between segments by swapping their seg-of.
;;   CYCLE-2 / CYCLE-2-WITH-ANALYSIS stay as static relations.
;;   NOTE: the CYCLE-4 actions are omitted (this minimal instance defines only a
;;   CYCLE-2); they would swap four cars cyclically over seg-of in the same way.

(define (domain scanalyzer3d-xts)
    (:requirements :typing :object-fluents)
    (:types segment car - object)
    (:predicates
        (analyzed ?c - car)
        (CYCLE-2 ?s1 ?s2 - segment)
        (CYCLE-2-WITH-ANALYSIS ?s1 ?s2 - segment)
    )
    (:functions
        (seg-of ?c - car) - segment
    )

    (:action analyze-2
        :parameters (?s1 ?s2 - segment ?c1 ?c2 - car)
        :precondition (and (CYCLE-2-WITH-ANALYSIS ?s1 ?s2)
                           (= (seg-of ?c1) ?s1) (= (seg-of ?c2) ?s2))
        :effect (and (assign (seg-of ?c1) ?s2) (assign (seg-of ?c2) ?s1)
                     (analyzed ?c1))
    )
    (:action rotate-2
        :parameters (?s1 ?s2 - segment ?c1 ?c2 - car)
        :precondition (and (CYCLE-2 ?s1 ?s2)
                           (= (seg-of ?c1) ?s1) (= (seg-of ?c2) ?s2))
        :effect (and (assign (seg-of ?c1) ?s2) (assign (seg-of ?c2) ?s1))
    )
)
