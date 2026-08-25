(define (problem X-sem-add-wrong-elem-type-01)
    (:domain X-sem-add-wrong-elem-type)
    (:init
        (= (int_bag) (set.mk (1 3)))
        (= (obj_bag) (set.mk (item_a)))
    )
    ;; Only put_obj_in_int_set can place item_a into int_bag (cross-type add).
    ;; If that action is correctly rejected, this goal is unsolvable.
    ;; If silently accepted, int_bag gains item_a and the goal is solved — reveals bug.
    (:goal (member item_a (int_bag)))
)
