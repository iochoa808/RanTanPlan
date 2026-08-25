;; seats = [[alice, bob], [carol, dave]].
;; Goal: seats[0][0] = bob AND seats[0][1] = alice  (row 0 swapped).
;;
;; Plan: swap_row(0, alice, bob)  — 1 step.

(define (problem seating-2d-01)
    (:domain seating-2d)

    (:init
        (= (seats) (array.mk ((alice bob)
                               (carol dave)))
        )
    )

    (:goal (and
        (= (read (seats) 0 0) bob)
        (= (read (seats) 0 1) alice)
    ))
)
