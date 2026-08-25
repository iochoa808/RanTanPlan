;; Test: two separate actions each writing once to the same array.
;;
;; Complements two_writes_one_action (two writes in a single action).
;; Here the writes happen across two distinct action steps, verifying
;; that frame axioms correctly preserve untouched cells between steps.
;;
;; ITE/store result after both steps fire:
;;   cells = store(store([0,0,0,0], 0, 5), 2, 7)
;; Cells 1 and 3 must stay 0 throughout.

(define (domain two-explicit-writes)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        idx   - (number 0 3)
        val   - (number 0 9)
        arr_t - (array 4 val)
    )

    (:functions
        (cells) - arr_t
    )

    (:action write_a
        :parameters ()
        :precondition (= (read (cells) 0) 0)
        :effect (write (cells) (0) 5)
    )

    (:action write_b
        :parameters ()
        :precondition (and
            (= (read (cells) 0) 5)
            (= (read (cells) 2) 0)
        )
        :effect (write (cells) (2) 7)
    )
)
