;; PDDL-XTS translation of pddl/test/woodworking-sequential-optimal-strips.
;; FEATURE: object fluents.
;;   Six functional "attribute" predicates each become an object fluent, turning
;;   every (not (attr ?x old))(attr ?x new) pair into a single assign:
;;     (surface-condition ?o ?s) -> (surface-of ?o)   - surface
;;     (wood ?o ?w)              -> (wood-of ?o)       - awood
;;     (treatment ?p ?t)         -> (treatment-of ?p)  - treatmentstatus
;;     (colour ?p ?c)            -> (colour-of ?p)      - acolour
;;     (boardsize ?b ?s)         -> (boardsize-of ?b)   - aboardsize
;;     (goalsize ?p ?s)          -> (goalsize-of ?p)    - apartsize (static)
;;   has-colour stays a predicate (partial: saws/grinders have no colour).
;;   Static successor relations and boolean status predicates stay as predicates.
;;   Object fluents are never passed directly as predicate arguments (function
;;   composition is rejected): `(is-smooth (surface-of ?x))` is written as an
;;   auxiliary parameter plus an equality, `(= (surface-of ?x) ?s) (is-smooth ?s)`,
;;   and likewise for `(grind-treatment-change (treatment-of ?x) ?new)`.
;;   Cost metric dropped (satisficing).

(define (domain woodworking-xts)
    (:requirements :typing :object-fluents)
    (:types
        acolour awood woodobj machine surface treatmentstatus aboardsize apartsize - object
        highspeed-saw glazer grinder immersion-varnisher planer saw spray-varnisher - machine
        board part - woodobj
    )
    (:constants
        verysmooth smooth rough - surface
        varnished glazed untreated colourfragments - treatmentstatus
        natural - acolour
        small medium large - apartsize
    )
    (:predicates
        (unused ?obj - part)
        (available ?obj - woodobj)
        (boardsize-successor ?size1 ?size2 - aboardsize)
        (in-highspeed-saw ?b - board ?m - highspeed-saw)
        (empty ?m - highspeed-saw)
        (has-colour ?machine - machine ?colour - acolour)
        (contains-part ?b - board ?p - part)
        (grind-treatment-change ?old ?new - treatmentstatus)
        (is-smooth ?surface - surface)
    )
    (:functions
        (surface-of ?o - woodobj)  - surface
        (wood-of ?o - woodobj)     - awood
        (treatment-of ?p - part)   - treatmentstatus
        (colour-of ?p - part)      - acolour
        (boardsize-of ?b - board)  - aboardsize
        (goalsize-of ?p - part)    - apartsize
    )

    (:action do-immersion-varnish
        :parameters (?x - part ?m - immersion-varnisher ?newcolour - acolour ?s - surface)
        :precondition (and (available ?x) (has-colour ?m ?newcolour)
                           (= (surface-of ?x) ?s) (is-smooth ?s)
                           (= (treatment-of ?x) untreated))
        :effect (and (assign (treatment-of ?x) varnished) (assign (colour-of ?x) ?newcolour))
    )
    (:action do-spray-varnish
        :parameters (?x - part ?m - spray-varnisher ?newcolour - acolour ?s - surface)
        :precondition (and (available ?x) (has-colour ?m ?newcolour)
                           (= (surface-of ?x) ?s) (is-smooth ?s)
                           (= (treatment-of ?x) untreated))
        :effect (and (assign (treatment-of ?x) varnished) (assign (colour-of ?x) ?newcolour))
    )
    (:action do-glaze
        :parameters (?x - part ?m - glazer ?newcolour - acolour)
        :precondition (and (available ?x) (has-colour ?m ?newcolour) (= (treatment-of ?x) untreated))
        :effect (and (assign (treatment-of ?x) glazed) (assign (colour-of ?x) ?newcolour))
    )
    (:action do-grind
        :parameters (?x - part ?m - grinder ?newtreatment - treatmentstatus
                     ?s - surface ?oldtreatment - treatmentstatus)
        :precondition (and (available ?x)
                           (= (surface-of ?x) ?s) (is-smooth ?s)
                           (= (treatment-of ?x) ?oldtreatment)
                           (grind-treatment-change ?oldtreatment ?newtreatment))
        :effect (and (assign (surface-of ?x) verysmooth) (assign (treatment-of ?x) ?newtreatment)
                     (assign (colour-of ?x) natural))
    )
    (:action do-plane
        :parameters (?x - part ?m - planer)
        :precondition (available ?x)
        :effect (and (assign (surface-of ?x) smooth) (assign (treatment-of ?x) untreated)
                     (assign (colour-of ?x) natural))
    )
    (:action load-highspeed-saw
        :parameters (?b - board ?m - highspeed-saw)
        :precondition (and (empty ?m) (available ?b))
        :effect (and (not (available ?b)) (not (empty ?m)) (in-highspeed-saw ?b ?m))
    )
    (:action unload-highspeed-saw
        :parameters (?b - board ?m - highspeed-saw)
        :precondition (in-highspeed-saw ?b ?m)
        :effect (and (available ?b) (not (in-highspeed-saw ?b ?m)) (empty ?m))
    )
    (:action cut-board-small
        :parameters (?b - board ?p - part ?m - highspeed-saw ?size_before ?size_after - aboardsize)
        :precondition (and (unused ?p) (= (goalsize-of ?p) small) (in-highspeed-saw ?b ?m)
                           (= (boardsize-of ?b) ?size_before) (boardsize-successor ?size_after ?size_before))
        :effect (and (not (unused ?p)) (available ?p) (assign (wood-of ?p) (wood-of ?b))
                     (assign (surface-of ?p) (surface-of ?b)) (assign (colour-of ?p) natural)
                     (assign (treatment-of ?p) untreated) (assign (boardsize-of ?b) ?size_after))
    )
    (:action cut-board-medium
        :parameters (?b - board ?p - part ?m - highspeed-saw ?size_before ?s1 ?size_after - aboardsize)
        :precondition (and (unused ?p) (= (goalsize-of ?p) medium) (in-highspeed-saw ?b ?m)
                           (= (boardsize-of ?b) ?size_before) (boardsize-successor ?size_after ?s1)
                           (boardsize-successor ?s1 ?size_before))
        :effect (and (not (unused ?p)) (available ?p) (assign (wood-of ?p) (wood-of ?b))
                     (assign (surface-of ?p) (surface-of ?b)) (assign (colour-of ?p) natural)
                     (assign (treatment-of ?p) untreated) (assign (boardsize-of ?b) ?size_after))
    )
    (:action cut-board-large
        :parameters (?b - board ?p - part ?m - highspeed-saw ?size_before ?s1 ?s2 ?size_after - aboardsize)
        :precondition (and (unused ?p) (= (goalsize-of ?p) large) (in-highspeed-saw ?b ?m)
                           (= (boardsize-of ?b) ?size_before) (boardsize-successor ?size_after ?s1)
                           (boardsize-successor ?s1 ?s2) (boardsize-successor ?s2 ?size_before))
        :effect (and (not (unused ?p)) (available ?p) (assign (wood-of ?p) (wood-of ?b))
                     (assign (surface-of ?p) (surface-of ?b)) (assign (colour-of ?p) natural)
                     (assign (treatment-of ?p) untreated) (assign (boardsize-of ?b) ?size_after))
    )
    (:action do-saw-small
        :parameters (?b - board ?p - part ?m - saw ?size_before ?size_after - aboardsize)
        :precondition (and (unused ?p) (= (goalsize-of ?p) small) (available ?b)
                           (= (boardsize-of ?b) ?size_before) (boardsize-successor ?size_after ?size_before))
        :effect (and (not (unused ?p)) (available ?p) (assign (wood-of ?p) (wood-of ?b))
                     (assign (surface-of ?p) (surface-of ?b)) (assign (colour-of ?p) natural)
                     (assign (treatment-of ?p) untreated) (assign (boardsize-of ?b) ?size_after))
    )
    (:action do-saw-medium
        :parameters (?b - board ?p - part ?m - saw ?size_before ?s1 ?size_after - aboardsize)
        :precondition (and (unused ?p) (= (goalsize-of ?p) medium) (available ?b)
                           (= (boardsize-of ?b) ?size_before) (boardsize-successor ?size_after ?s1)
                           (boardsize-successor ?s1 ?size_before))
        :effect (and (not (unused ?p)) (available ?p) (assign (wood-of ?p) (wood-of ?b))
                     (assign (surface-of ?p) (surface-of ?b)) (assign (colour-of ?p) natural)
                     (assign (treatment-of ?p) untreated) (assign (boardsize-of ?b) ?size_after))
    )
    (:action do-saw-large
        :parameters (?b - board ?p - part ?m - saw ?size_before ?s1 ?s2 ?size_after - aboardsize)
        :precondition (and (unused ?p) (= (goalsize-of ?p) large) (available ?b)
                           (= (boardsize-of ?b) ?size_before) (boardsize-successor ?size_after ?s1)
                           (boardsize-successor ?s1 ?s2) (boardsize-successor ?s2 ?size_before))
        :effect (and (not (unused ?p)) (available ?p) (assign (wood-of ?p) (wood-of ?b))
                     (assign (surface-of ?p) (surface-of ?b)) (assign (colour-of ?p) natural)
                     (assign (treatment-of ?p) untreated) (assign (boardsize-of ?b) ?size_after))
    )
)
