;; Problem: sort the canvas from [green, blue, red] to [red, green, blue].
;;
;; Plan: swap(0,2,green,red) → [red,blue,green]
;;       swap(1,2,blue,green) → [red,green,blue]   — 2 steps.

(define (problem color-palette-01)
    (:domain color-palette)

    (:init
        (= (canvas) (array.mk (green blue red)))
    )

    (:goal
        (= (canvas) (array.mk (red green blue)))
    )
)
