(define (problem sdac-simple-1)
  (:domain sdac-simple)
  (:objects
    c0 - counter
  )
  (:init
    (= (value c0) 1)
    (= (total-cost) 0)
    (is-target c0)
  )
  (:goal (done))
  (:metric minimize (total-cost))
)
