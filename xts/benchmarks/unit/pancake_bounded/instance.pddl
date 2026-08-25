(define (problem pancake-p1)

  (:domain pancake-int)

  (:init
    ;; initial scrambled stack: [3 1 4 0 2]
    (= (val 0) 3)
    (= (val 1) 1)
    (= (val 2) 4)
    (= (val 3) 0)
    (= (val 4) 2)
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
