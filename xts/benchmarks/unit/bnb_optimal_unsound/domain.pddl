;; KNOWN-BUG REPRODUCER (array soundness under B&B / abstract-suffix encoder).
;;
;; This domain is VALID and trivially solvable: one action copies src into dst.
;;
;;   Default (satisficing seq):   SOLVED, 1 step (copy).
;;   Optimal mode (--mode optimal => branch_and_bound + abstract-suffix):
;;                                 *** UNSOLVABLE_PROVEN *** at T0  (WRONG).
;;
;; Root cause (see docs/ChangesInPipeline.md §5.5, fileByFile.md
;; abstract_suffix_encoder caveat): AbstractSuffixEncoder::encode_mod_v_equivalences
;; builds "fluent v can be modified" literals from epc_index_ only.  Array
;; writes are recorded exclusively in array_epc_index_, so for the array fluent
;; `dst` the encoder asserts ¬mod_dst ("dst can never change").  The abstract
;; suffix then concludes the goal on dst is unreachable and proves the problem
;; UNSOLVABLE even though a 1-step plan exists.
;;
;; To reproduce the bug:
;;   solve.py -d domain.pddl -p instance.pddl --mode optimal
;; Expected (correct): SOLVED optimal, 1 step.   Actual: UNSOLVABLE_PROVEN.
;;
;; The auto-runner exercises this folder in the DEFAULT (seq satisficing)
;; strategy, where it correctly SOLVES — so the suite stays green; the bug is
;; only triggered by the --mode optimal command above.

(define (domain bnb-optimal-unsound)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val   - (number 0 9)
        arr_t - (array 2 val)
    )

    (:functions
        (src) - arr_t
        (dst) - arr_t
    )

    (:action copy
        :parameters ()
        :effect (assign (dst) (src))
    )
)
