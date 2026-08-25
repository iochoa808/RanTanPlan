;; arr = all zeros (4×3×2×1 = 24 cells).
;;
;; Each goal cell exercises a DIFFERENT dimension at its maximum index:
;;   arr[3][0][0][0] = 1   (d0 at max=3; if d0/d1 swapped → need d1=3, out of range → UNSOLVABLE)
;;   arr[0][2][0][0] = 2   (d1 at max=2; if d1/d2 swapped → need d2=2, out of range → UNSOLVABLE)
;;   arr[0][0][1][0] = 3   (d2 at max=1; if d2/d3 swapped → need d3=1, out of range → UNSOLVABLE)
;;
;; Expected plan (3 steps):
;;   set(3,0,0,0,1), set(0,2,0,0,2), set(0,0,1,0,3)

(define (problem test-4d-order-01)
    (:domain test-4d-order)

    (:init
        ;; Layout: arr[d0][d1][d2][d3]
        ;;   outer grouping = d0 (4 groups)
        ;;   next           = d1 (3 groups each)
        ;;   next           = d2 (2 groups each)
        ;;   innermost      = d3 (1 value each, in parens)
        (= (arr) (array.mk ((((0)(0))((0)(0))((0)(0)))
                             (((0)(0))((0)(0))((0)(0)))
                             (((0)(0))((0)(0))((0)(0)))
                             (((0)(0))((0)(0))((0)(0))))))
    )

    (:goal
        (and
            (= (read (arr) (3) (0) (0) (0)) 1)
            (= (read (arr) (0) (2) (0) (0)) 2)
            (= (read (arr) (0) (0) (1) (0)) 3)
        )
    )
)
