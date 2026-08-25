;; PDDL-XTS translation of pddl/test/child-snack .../problem.pddl (simple-child-snack).
(define (problem simple-child-snack-xts)
    (:domain child-snack-xts)
    (:objects
        child1 child2 - child
        bread1 bread2 - bread-portion
        content1 content2 - content-portion
        tray1 - tray
        table1 - place
        sandw1 sandw2 - sandwich
    )
    (:init
        (= (tray-at tray1) kitchen)
        (at_kitchen_bread bread1) (at_kitchen_bread bread2)
        (at_kitchen_content content1) (at_kitchen_content content2)
        (no_gluten_bread bread2) (no_gluten_content content2)
        (allergic_gluten child1) (not_allergic_gluten child2)
        (= (child-at child1) table1) (= (child-at child2) table1)
        (notexist sandw1) (notexist sandw2)
    )
    (:goal (and (served child1) (served child2)))
)
