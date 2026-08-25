;; rooms = [{red, blue}, {green}, {red}].
;; Goal: room 2 contains red AND green.
;;
;; Plan: gather(1, 2)  — 1 step.
;; After: room 2 = union({red}, {green}) = {red, green}.

(define (problem tag-row-01)
    (:domain tag-row)

    (:init
        (= (rooms) (array.mk ((set.mk (red blue))
                              (set.mk (green))
                              (set.mk (red)))))
    )

    (:goal (and
        (member red   (read (rooms) 2))
        (member green (read (rooms) 2))
    ))
)
