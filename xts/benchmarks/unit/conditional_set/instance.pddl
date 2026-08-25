;; Items: alpha, beta, gamma.  gamma is quarantined.
;; pool starts empty.
;;
;; Goal: alpha ∈ pool AND beta ∈ pool.
;;
;; Plan: admit(alpha), admit(beta)  — 2 steps.
;; (admit(gamma) would fire the action but the conditional effect does nothing.)

(define (problem cond-set-01)
    (:domain cond-set)

    (:objects
        alpha beta gamma - item
    )

    (:init
        (= (pool) (set.mk ()))
        (quarantined gamma)
    )

    (:goal (and
        (member alpha (pool))
        (member beta  (pool))
    ))
)
