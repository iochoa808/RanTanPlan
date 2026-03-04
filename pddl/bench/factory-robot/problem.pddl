;; Factory Robot — 2 robots, 3 stations
;; Robots must accumulate workload while managing energy and heat.

(define (problem factory-1)
  (:domain factory-robot)

  (:objects
    r1 r2 - robot
    assembly charging cooling - station
  )

  (:init
    ;; Positions
    (at r1 assembly)
    (at r2 charging)

    ;; Connectivity (fully connected)
    (connected assembly charging)
    (connected charging assembly)
    (connected assembly cooling)
    (connected cooling assembly)
    (connected charging cooling)
    (connected cooling charging)

    ;; Station equipment
    (has-charger charging)
    (has-calibrator cooling)

    ;; Station occupancy (cooling is free)
    (free cooling)

    ;; Robots start calibrated
    (calibrated r1)
    (calibrated r2)

    ;; Robot state (modified fluents)
    (= (energy r1) 100)
    (= (energy r2) 80)
    (= (workload r1) 0)
    (= (workload r2) 0)
    (= (temperature r1) 0)
    (= (temperature r2) 0)
    (= (production r1) 0)
    (= (production r2) 0)

    ;; Constants (never modified — used by freeze analysis)
    (= (capacity r1) 100)
    (= (capacity r2) 120)
    (= (work-cost r1) 10)
    (= (work-cost r2) 15)
    (= (cooling-power assembly) 0)
    (= (cooling-power charging) 0)
    (= (cooling-power cooling) 5)
    (= (max-temp r1) 15)
    (= (max-temp r2) 20)
    (= (efficiency r1) 2)
    (= (efficiency r2) 3)
  )

  (:goal (and
    (>= (workload r1) 12)
    (<= (temperature r1) 3)
    (>= (workload r2) 3)
  ))
)
