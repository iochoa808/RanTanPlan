;; PDDL-XTS translation of pddl/test/cave-diving-sequential-optimal/problem.pddl.
;; quantity objects (zero one two three) and the next-quantity chain are removed;
;; initial capacity three -> (= (cap d0) 3).
;; at-surface d0 -> (= (diver-at d0) surface-loc)  (domain constant, not an object).
;; connected l0 l1 / l1 l0 -> symmetric connects sets.

(define (problem minimal-cave-diving-xts)
    (:domain cave-diving-xts)
    (:objects
        l0 l1 - location
        d0 - diver
        t0 t1 t2 t3 dummy - tank
    )
    (:init
        (available d0)
        (= (cap d0) 3)
        (= (diver-at d0) surface-loc)

        (in-storage t0)
        (next-tank t0 t1)
        (next-tank t1 t2)
        (next-tank t2 t3)
        (next-tank t3 dummy)

        (cave-entrance l0)
        (= (connects l0) (set.mk (l1)))
        (= (connects l1) (set.mk (l0)))
        (= (connects surface-loc) (set.mk ()))
    )
    (:goal (and (have-photo l1) (decompressing d0)))
)