(define (problem counter-p1)
    (:domain counter)

    (:objects)

    (:init
        (= (counter) 0)
    )

    ;; counter=5 unreachable via set-counter alone (max set-val=4);
    ;; requires set-counter then inc.
    ;; Plan: set-counter(4), inc(1)  — 2 steps.
    (:goal
        (= (counter) 5)
    )
)
