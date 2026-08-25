;; Initial grid:
;;
;;   col:   0          1           2               3
;;   row 0: {W,E}      {S,W}       {N,W,S,E}       {N,W,E}
;;   row 1: {N,W,E}    {S,W}       {N,S}           {S,E}
;;   row 2: {N,S,E}    {S,W}       {N,S,E}         {W,E}
;;   row 3: {N,W}      {N,W,S}     {N,S}           {S,W}

(define (problem labyrinth-i0-v2) (:domain labyrinth2)

    (:init
        (= (card_at) (array.mk (((set.mk (W E))     (set.mk (S W))  (set.mk (N W S E))  (set.mk (N W E)))
                                 ((set.mk (N W E))  (set.mk (S W))  (set.mk (N S))      (set.mk (S E)))
                                 ((set.mk (N S E))  (set.mk (S W))  (set.mk (N S E))    (set.mk (S E)))
                                 ((set.mk (N W))    (set.mk (N W S))(set.mk (N S))      (set.mk (S W))))))

        (= (robot-row) 0)
        (= (robot-col) 0)
    )

    (:goal (and
        (= (robot-row) 3)
        (= (robot-col) 3)
        (member S (read (card_at) 3 3))
    ))
)
