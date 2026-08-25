;; BREAK TARGET: state fluent used as an array index.
;;
;; The spec (pitfalls table, section 7) states:
;;   "Fluent-valued index (read (arr) (head)) — Not supported —
;;    the index must be a constant or action parameter, not a state fluent."
;;
;; The action `copy_head` reads the scalar fluent (head) and passes its
;; result directly as the index into a read of (cells).  This is indirect
;; function composition that the compiler cannot resolve statically.
;;
;; Expected: semantic/compilation error detecting the fluent-valued index.

(define (domain X-fluent-as-array-index)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        idx  - (number 0 3)
        val  - (number 0 9)
        arr  - (array 4 val)
    )

    (:functions
        (cells)  - arr
        (head)   - idx    ; scalar index fluent — NOT a valid array index
        (result) - val
    )

    ;; read using (head) as index — must be rejected
    (:action copy_head
        :parameters ()
        :precondition ()
        :effect (assign (result) (read (cells) (head)))
    )
)
