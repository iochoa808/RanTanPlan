;; SEMANTIC BREAK: a `forall` nested directly inside another `forall` in an
;; :effect body.
;;
;; The reader explicitly rejects this ("Nested forall on effects are not
;; supported.").  A 2-D wipe must instead be written as a single forall whose
;; body addresses both indices, or as separate effects.  This test locks in
;; that rejection so a future change cannot silently start accepting (and
;; mis-expanding) nested-forall effects.
;;
;; Expected: parse/semantic error — nested forall in effect.

(define (domain X-nested-forall-effect)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val   - (number 0 5)
        row_t - (array 2 val)
        grid_t - (array 2 row_t)
    )

    (:functions
        (g) - grid_t
    )

    (:action zero_all
        :parameters ()
        :effect (forall (?i - (number 0 1))
                  (forall (?j - (number 0 1))
                    (write ((g) ?i ?j) 0)))
    )
)
