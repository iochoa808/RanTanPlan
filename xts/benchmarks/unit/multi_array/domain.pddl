;; Test: two independent ARRAY FLUENTS in the same domain.
;;
;; KEY FEATURE: reading from one array and writing to another.
;; All previous array tests use a single array fluent per domain.
;;
;; Domain: copy a source array into a destination array, slot by slot.
;; Constant indices keep grounding simple (ObjectFluentIndex handles ?v).

(define (domain dual-array)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot - (number 0 2)
        val  - (number 0 4)
        arr  - (array 3 val)
    )

    (:functions
        (src) - arr
        (dst) - arr
    )

    (:action copy_0
        :parameters (?v - val)
        :precondition (and
            (= (read (src) 0) ?v)
            (not (= (read (dst) 0) ?v))
        )
        :effect (write (dst) (0) ?v)
    )

    (:action copy_1
        :parameters (?v - val)
        :precondition (and
            (= (read (src) 1) ?v)
            (not (= (read (dst) 1) ?v))
        )
        :effect (write (dst) (1) ?v)
    )

    (:action copy_2
        :parameters (?v - val)
        :precondition (and
            (= (read (src) 2) ?v)
            (not (= (read (dst) 2) ?v))
        )
        :effect (write (dst) (2) ?v)
    )
)
