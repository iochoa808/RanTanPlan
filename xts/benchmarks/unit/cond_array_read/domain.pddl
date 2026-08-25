;; Test: conditional effect whose `when` CONDITION reads an array cell.
;;
;; KEY PATTERN: conditional_array gates a (when ...) on a plain boolean
;;   predicate; here the condition is (> (read (cells) ?i) 5), so the
;;   array-read must be encoded inside the conditional-effect guard (not just
;;   in a top-level precondition or effect value).

(define (domain cond-arrread)
    (:requirements :typing :arrays :bounded-integers :conditional-effects)

    (:types
        idx   - (number 0 2)
        val   - (number 0 9)
        cnt   - (number 0 9)
        arr_t - (array 3 val)
    )

    (:functions
        (cells) - arr_t
        (hits)  - cnt
    )

    ;; Count, conditionally, cells whose value exceeds 5.
    (:action scan
        :parameters (?i - idx)
        :effect (when (> (read (cells) ?i) 5) (increase (hits) 1))
    )
)
