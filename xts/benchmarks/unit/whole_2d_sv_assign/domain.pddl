;; Test: whole-array SV-to-SV ASSIGN for a 2D array.
;;
;; KEY PATTERN: (assign (dst) (src)) where both are 2D array fluents.
;;   whole_sv_assign tests this for 1D; this extends the coverage to 2D.
;;   The value side is a live 2D array variable (not an array.mk literal),
;;   exercising the SV-valued replacement path for multidimensional arrays.
;;
;; Domain: src and dst are 2×3 integer arrays.
;;   copy_all() copies the entire src into dst in one effect.
;; Initial: src=[[1,2,3],[4,5,6]], dst=[[0,0,0],[0,0,0]].
;; Goal: dst[1][2] = 6.
;; Plan: copy_all()  — 1 step.

(define (domain sv-assign-2d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        row   - (number 0 1)
        col   - (number 0 2)
        val   - (number 0 9)
        mat_t - (array 2 3 val)
    )

    (:functions
        (src) - mat_t
        (dst) - mat_t
    )

    ;; Copy entire 2D src into dst.
    (:action copy_all
        :parameters ()
        :precondition (= (read (dst) 0 0) 0)
        :effect (assign (dst) (src))
    )
)
