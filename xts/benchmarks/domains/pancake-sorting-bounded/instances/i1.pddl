(define (problem pancake-p1)

  (:domain pancake-int)

  (:init
    (= (val 0) 3)
    (= (val 1) 0)
    (= (val 2) 1)
    (= (val 3) 2)
    (= (val 4) 4)
  )

  (:goal
    (and
      ;; sorted ascending: [0 1 2 3 4]
      (= (val 0) 0)
      (= (val 1) 1)
      (= (val 2) 2)
      (= (val 3) 3)
      (= (val 4) 4)
    )
  )
)
