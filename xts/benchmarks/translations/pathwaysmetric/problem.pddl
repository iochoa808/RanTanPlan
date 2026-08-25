;; PDDL-XTS translation of pddl/test/pathwaysmetric/problem.pddl (Simple-Pathways-Metric).
;; The real (duration-association-reaction ... 1.0) fact is dropped.
(define (problem Simple-Pathways-Metric-xts)
    (:domain pathways-metric-xts)
    (:objects
        pCAF p300 - simple
        pCAF-p300 - complex
    )
    (:init
        (possible pCAF) (= (available pCAF) 0)
        (possible p300) (= (available p300) 0)
        (= (available pCAF-p300) 0)
        (association-reaction pCAF p300 pCAF-p300)
        (= (need-for-association pCAF p300 pCAF-p300) 1)
        (= (need-for-association p300 pCAF pCAF-p300) 1)
        (= (prod-by-association pCAF p300 pCAF-p300) 1)
        (= (num-subs) 0)
    )
    (:goal (>= (available pCAF-p300) 1))
)
