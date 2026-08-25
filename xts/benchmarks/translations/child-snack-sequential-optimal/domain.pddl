;; PDDL-XTS translation of pddl/test/child-snack-sequential-optimal.
;; FEATURE: object fluents.
;;   - (at ?t ?p) tray location (total function) -> object fluent (tray-at ?t) - place.
;;   - (waiting ?c ?p) child's place (total function) -> object fluent (child-at ?c) - place.
;;     Serving a sandwich then requires (= (child-at ?c) (tray-at ?t)).
;;   Gluten flags, sandwich existence/location and `served` stay boolean.

(define (domain child-snack-xts)
    (:requirements :typing :equality :object-fluents)
    (:types child bread-portion content-portion sandwich tray place)
    (:constants kitchen - place)

    (:predicates
        (at_kitchen_bread ?b - bread-portion)
        (at_kitchen_content ?c - content-portion)
        (at_kitchen_sandwich ?s - sandwich)
        (no_gluten_bread ?b - bread-portion)
        (no_gluten_content ?c - content-portion)
        (ontray ?s - sandwich ?t - tray)
        (no_gluten_sandwich ?s - sandwich)
        (allergic_gluten ?c - child)
        (not_allergic_gluten ?c - child)
        (served ?c - child)
        (notexist ?s - sandwich)
    )
    (:functions
        (tray-at ?t - tray)   - place
        (child-at ?c - child) - place
    )

    (:action make_sandwich_no_gluten
        :parameters (?s - sandwich ?b - bread-portion ?c - content-portion)
        :precondition (and (at_kitchen_bread ?b) (at_kitchen_content ?c)
                           (no_gluten_bread ?b) (no_gluten_content ?c) (notexist ?s))
        :effect (and (not (at_kitchen_bread ?b)) (not (at_kitchen_content ?c))
                     (at_kitchen_sandwich ?s) (no_gluten_sandwich ?s) (not (notexist ?s)))
    )
    (:action make_sandwich
        :parameters (?s - sandwich ?b - bread-portion ?c - content-portion)
        :precondition (and (at_kitchen_bread ?b) (at_kitchen_content ?c) (notexist ?s))
        :effect (and (not (at_kitchen_bread ?b)) (not (at_kitchen_content ?c))
                     (at_kitchen_sandwich ?s) (not (notexist ?s)))
    )
    (:action put_on_tray
        :parameters (?s - sandwich ?t - tray)
        :precondition (and (at_kitchen_sandwich ?s) (= (tray-at ?t) kitchen))
        :effect (and (not (at_kitchen_sandwich ?s)) (ontray ?s ?t))
    )
    (:action serve_sandwich_no_gluten
        :parameters (?s - sandwich ?c - child ?t - tray)
        :precondition (and (allergic_gluten ?c) (ontray ?s ?t)
                           (no_gluten_sandwich ?s) (= (child-at ?c) (tray-at ?t)))
        :effect (and (not (ontray ?s ?t)) (served ?c))
    )
    (:action serve_sandwich
        :parameters (?s - sandwich ?c - child ?t - tray)
        :precondition (and (not_allergic_gluten ?c) (ontray ?s ?t)
                           (= (child-at ?c) (tray-at ?t)))
        :effect (and (not (ontray ?s ?t)) (served ?c))
    )
    (:action move_tray
        :parameters (?t - tray ?p2 - place)
        :precondition ()
        :effect (assign (tray-at ?t) ?p2)
    )
)
