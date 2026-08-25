;; BREAK TARGET: constant literal index that is statically beyond the
;; declared array size.
;;
;; `store` is (array 4 val) — valid indices are 0, 1, 2, 3.
;; The actions `read_oob` and `write_oob` use the literal constant 4 and 7
;; as indices — both are statically out of bounds and should be rejected.
;;
;; This is a pure static check: unlike computed-index OOB (which requires
;; analysing parameter ranges), the indices here are integer literals so
;; the compiler has all the information it needs at parse time.
;;
;; Expected: error for both literal out-of-bounds index accesses.

(define (domain X-array-const-index-oob)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 3)    ; valid indices: 0..3
        val   - (number 0 9)
        store - (array 4 val)   ; size 4 → indices 0..3
    )

    (:functions
        (cells) - store
        (result) - val
    )

    ;; read at index 4 — one past the end
    (:action read_oob
        :parameters ()
        :precondition ()
        :effect (assign (result) (read (cells) 4))
    )

    ;; write at index 7 — well past the end
    (:action write_oob
        :parameters ()
        :precondition ()
        :effect (write (cells) (7) 0)
    )
)
