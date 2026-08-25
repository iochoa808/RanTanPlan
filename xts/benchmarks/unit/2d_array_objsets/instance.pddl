;; labels row 0 = [{red,blue}, {green}, {blue,green}].
;; labels row 1 = [{},         {},      {}          ].
;; active_c = {1, 2}.
;;
;; Goal: green ∈ labels[1][1]  AND  blue ∈ labels[1][2].
;;
;; Plan: propagate(1), propagate(2)  — 2 steps.

(define (problem label-grid-01)
    (:domain label-grid)

    (:init
        (= (labels)   (array.mk (((set.mk (red blue))  (set.mk (green))      (set.mk (blue green)))
                                  ((set.mk ())          (set.mk ())           (set.mk ())))))
        (= (active_c) (set.mk (1 2)))
    )

    (:goal (and
        (member green (read (labels) 1 1))
        (member blue  (read (labels) 1 2))
    ))
)
