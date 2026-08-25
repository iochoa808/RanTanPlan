;; Problem: route tasks 3 and 4 to bay 2.
;;
;; Initial: bay 0 = {0,1,2}, bay 1 = {3,4}, bay 2 = {0}.
;; Goal:    bay 2 contains tasks 3 and 4.
;; Plan:    copy_tasks(1, 2)  — 1 step.

(define (problem task-router-01)
    (:domain task-router)

    (:init
        (= (assignments) (array.mk ((set.mk (0 1 2))
                                    (set.mk (3 4))
                                    (set.mk (0)))))
    )

    (:goal (and
        (member 3 (read (assignments) 2))
        (member 4 (read (assignments) 2))
    ))
)
