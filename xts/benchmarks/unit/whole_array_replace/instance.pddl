;; tape = [5, 6, 7] (neither zeros nor the target pattern).
;; Goal: tape = [1, 2, 3].
;;
;; load_pattern replaces the whole array in one step.
;; zero_tape is a distractor — using it first then load_pattern is also valid
;; (optimal is just load_pattern directly).
;;
;; Expected plan (1 step): load_pattern()

(define (problem whole-replace-01)
    (:domain whole-replace)

    (:init
        (= (tape) (array.mk (5 6 7)))
    )

    (:goal
        (= (tape) (array.mk (1 2 3)))
    )
)
