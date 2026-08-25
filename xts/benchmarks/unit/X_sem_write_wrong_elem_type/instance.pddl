(define (problem X-sem-write-wrong-elem-type-01)
    (:domain X-sem-write-wrong-elem-type)
    (:init
        (= (int_arr) (array.mk (0 0 0)))
        (= (obj_arr) (array.mk (tok_a tok_b tok_c)))
    )
    (:goal (= (read (int_arr) 0) 1))
)
