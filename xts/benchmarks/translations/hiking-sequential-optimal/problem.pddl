;; PDDL-XTS translation of pddl/test/hiking .../problem.pddl (Hiking-1-2).
(define (problem Hiking-1-2-xts)
    (:domain hiking-xts)
    (:objects
        car0 car1 - car
        tent0 - tent
        couple0 - couple
        place0 place1 place2 - place
        guy0 girl0 - person
    )
    (:init
        (partners couple0 guy0 girl0)
        (= (person-at guy0) place0) (= (person-at girl0) place0)
        (= (walked-at couple0) place0)
        (= (tent-at tent0) place0) (up tent0)
        (= (car-at car0) place0) (= (car-at car1) place0)
        (next place0 place1) (next place1 place2)
    )
    (:goal (= (walked-at couple0) place2))
)
