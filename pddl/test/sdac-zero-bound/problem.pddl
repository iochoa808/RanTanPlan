(define (problem sdac-zero-bound-1)
  (:domain sdac-zero-bound)
  (:objects
    c0 - counter
  )
  (:init
    (= (value c0) 0)
    (= (total-cost) 0)
    (is-target c0)
  )
  (:goal (done))
  (:metric minimize (total-cost))
)
