;; chain = {8, 9}.  step_overflow(?n=9): add (9+1)=10 to chain — OOB.
(define (problem X-sem-arith-elem-exceeds-type-01)
    (:domain X-sem-arith-elem-exceeds-type)
    (:init
        (= (chain) (set.mk (8 9)))
    )
    ;; step_overflow can only add n+1 ∈ {9, 10} to chain.
    ;; 7 is not in init {8,9} and never produced by step_overflow → unreachable.
    ;; Test passes whether the domain is rejected (overflow check) or not (unsolvable).
    (:goal (member 7 (chain)))
)
