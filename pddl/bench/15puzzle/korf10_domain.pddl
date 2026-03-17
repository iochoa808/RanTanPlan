(define (domain arm_ipar_npuzzle_problem-domain)
 (:requirements :strips :equality :numeric-fluents :action-costs)
 (:functions 
             (puzzle_0_0)
             (puzzle_0_1)
             (puzzle_0_2)
             (puzzle_0_3)
             (puzzle_1_0)
             (puzzle_1_1)
             (puzzle_1_2)
             (puzzle_1_3)
             (puzzle_2_0)
             (puzzle_2_1)
             (puzzle_2_2)
             (puzzle_2_3)
             (puzzle_3_0)
             (puzzle_3_1)
             (puzzle_3_2)
             (puzzle_3_3)
             (total-cost)
 )
 (:action move_up_1_0
  :parameters ()
  :precondition (and (= (puzzle_0_0) 0))
  :effect (and (assign (puzzle_0_0) (puzzle_1_0)) (assign (puzzle_1_0) 0) (increase (total-cost) 1)))
 (:action move_up_1_1
  :parameters ()
  :precondition (and (= (puzzle_0_1) 0))
  :effect (and (assign (puzzle_0_1) (puzzle_1_1)) (assign (puzzle_1_1) 0) (increase (total-cost) 1)))
 (:action move_up_1_2
  :parameters ()
  :precondition (and (= (puzzle_0_2) 0))
  :effect (and (assign (puzzle_0_2) (puzzle_1_2)) (assign (puzzle_1_2) 0) (increase (total-cost) 1)))
 (:action move_up_1_3
  :parameters ()
  :precondition (and (= (puzzle_0_3) 0))
  :effect (and (assign (puzzle_0_3) (puzzle_1_3)) (assign (puzzle_1_3) 0) (increase (total-cost) 1)))
 (:action move_up_2_0
  :parameters ()
  :precondition (and (= (puzzle_1_0) 0))
  :effect (and (assign (puzzle_1_0) (puzzle_2_0)) (assign (puzzle_2_0) 0) (increase (total-cost) 1)))
 (:action move_up_2_1
  :parameters ()
  :precondition (and (= (puzzle_1_1) 0))
  :effect (and (assign (puzzle_1_1) (puzzle_2_1)) (assign (puzzle_2_1) 0) (increase (total-cost) 1)))
 (:action move_up_2_2
  :parameters ()
  :precondition (and (= (puzzle_1_2) 0))
  :effect (and (assign (puzzle_1_2) (puzzle_2_2)) (assign (puzzle_2_2) 0) (increase (total-cost) 1)))
 (:action move_up_2_3
  :parameters ()
  :precondition (and (= (puzzle_1_3) 0))
  :effect (and (assign (puzzle_1_3) (puzzle_2_3)) (assign (puzzle_2_3) 0) (increase (total-cost) 1)))
 (:action move_up_3_0
  :parameters ()
  :precondition (and (= (puzzle_2_0) 0))
  :effect (and (assign (puzzle_2_0) (puzzle_3_0)) (assign (puzzle_3_0) 0) (increase (total-cost) 1)))
 (:action move_up_3_1
  :parameters ()
  :precondition (and (= (puzzle_2_1) 0))
  :effect (and (assign (puzzle_2_1) (puzzle_3_1)) (assign (puzzle_3_1) 0) (increase (total-cost) 1)))
 (:action move_up_3_2
  :parameters ()
  :precondition (and (= (puzzle_2_2) 0))
  :effect (and (assign (puzzle_2_2) (puzzle_3_2)) (assign (puzzle_3_2) 0) (increase (total-cost) 1)))
 (:action move_up_3_3
  :parameters ()
  :precondition (and (= (puzzle_2_3) 0))
  :effect (and (assign (puzzle_2_3) (puzzle_3_3)) (assign (puzzle_3_3) 0) (increase (total-cost) 1)))
 (:action move_down_0_0
  :parameters ()
  :precondition (and (= (puzzle_1_0) 0))
  :effect (and (assign (puzzle_1_0) (puzzle_0_0)) (assign (puzzle_0_0) 0) (increase (total-cost) 1)))
 (:action move_down_0_1
  :parameters ()
  :precondition (and (= (puzzle_1_1) 0))
  :effect (and (assign (puzzle_1_1) (puzzle_0_1)) (assign (puzzle_0_1) 0) (increase (total-cost) 1)))
 (:action move_down_0_2
  :parameters ()
  :precondition (and (= (puzzle_1_2) 0))
  :effect (and (assign (puzzle_1_2) (puzzle_0_2)) (assign (puzzle_0_2) 0) (increase (total-cost) 1)))
 (:action move_down_0_3
  :parameters ()
  :precondition (and (= (puzzle_1_3) 0))
  :effect (and (assign (puzzle_1_3) (puzzle_0_3)) (assign (puzzle_0_3) 0) (increase (total-cost) 1)))
 (:action move_down_1_0
  :parameters ()
  :precondition (and (= (puzzle_2_0) 0))
  :effect (and (assign (puzzle_2_0) (puzzle_1_0)) (assign (puzzle_1_0) 0) (increase (total-cost) 1)))
 (:action move_down_1_1
  :parameters ()
  :precondition (and (= (puzzle_2_1) 0))
  :effect (and (assign (puzzle_2_1) (puzzle_1_1)) (assign (puzzle_1_1) 0) (increase (total-cost) 1)))
 (:action move_down_1_2
  :parameters ()
  :precondition (and (= (puzzle_2_2) 0))
  :effect (and (assign (puzzle_2_2) (puzzle_1_2)) (assign (puzzle_1_2) 0) (increase (total-cost) 1)))
 (:action move_down_1_3
  :parameters ()
  :precondition (and (= (puzzle_2_3) 0))
  :effect (and (assign (puzzle_2_3) (puzzle_1_3)) (assign (puzzle_1_3) 0) (increase (total-cost) 1)))
 (:action move_down_2_0
  :parameters ()
  :precondition (and (= (puzzle_3_0) 0))
  :effect (and (assign (puzzle_3_0) (puzzle_2_0)) (assign (puzzle_2_0) 0) (increase (total-cost) 1)))
 (:action move_down_2_1
  :parameters ()
  :precondition (and (= (puzzle_3_1) 0))
  :effect (and (assign (puzzle_3_1) (puzzle_2_1)) (assign (puzzle_2_1) 0) (increase (total-cost) 1)))
 (:action move_down_2_2
  :parameters ()
  :precondition (and (= (puzzle_3_2) 0))
  :effect (and (assign (puzzle_3_2) (puzzle_2_2)) (assign (puzzle_2_2) 0) (increase (total-cost) 1)))
 (:action move_down_2_3
  :parameters ()
  :precondition (and (= (puzzle_3_3) 0))
  :effect (and (assign (puzzle_3_3) (puzzle_2_3)) (assign (puzzle_2_3) 0) (increase (total-cost) 1)))
 (:action move_left_0_1
  :parameters ()
  :precondition (and (= (puzzle_0_0) 0))
  :effect (and (assign (puzzle_0_0) (puzzle_0_1)) (assign (puzzle_0_1) 0) (increase (total-cost) 1)))
 (:action move_left_0_2
  :parameters ()
  :precondition (and (= (puzzle_0_1) 0))
  :effect (and (assign (puzzle_0_1) (puzzle_0_2)) (assign (puzzle_0_2) 0) (increase (total-cost) 1)))
 (:action move_left_0_3
  :parameters ()
  :precondition (and (= (puzzle_0_2) 0))
  :effect (and (assign (puzzle_0_2) (puzzle_0_3)) (assign (puzzle_0_3) 0) (increase (total-cost) 1)))
 (:action move_left_1_1
  :parameters ()
  :precondition (and (= (puzzle_1_0) 0))
  :effect (and (assign (puzzle_1_0) (puzzle_1_1)) (assign (puzzle_1_1) 0) (increase (total-cost) 1)))
 (:action move_left_1_2
  :parameters ()
  :precondition (and (= (puzzle_1_1) 0))
  :effect (and (assign (puzzle_1_1) (puzzle_1_2)) (assign (puzzle_1_2) 0) (increase (total-cost) 1)))
 (:action move_left_1_3
  :parameters ()
  :precondition (and (= (puzzle_1_2) 0))
  :effect (and (assign (puzzle_1_2) (puzzle_1_3)) (assign (puzzle_1_3) 0) (increase (total-cost) 1)))
 (:action move_left_2_1
  :parameters ()
  :precondition (and (= (puzzle_2_0) 0))
  :effect (and (assign (puzzle_2_0) (puzzle_2_1)) (assign (puzzle_2_1) 0) (increase (total-cost) 1)))
 (:action move_left_2_2
  :parameters ()
  :precondition (and (= (puzzle_2_1) 0))
  :effect (and (assign (puzzle_2_1) (puzzle_2_2)) (assign (puzzle_2_2) 0) (increase (total-cost) 1)))
 (:action move_left_2_3
  :parameters ()
  :precondition (and (= (puzzle_2_2) 0))
  :effect (and (assign (puzzle_2_2) (puzzle_2_3)) (assign (puzzle_2_3) 0) (increase (total-cost) 1)))
 (:action move_left_3_1
  :parameters ()
  :precondition (and (= (puzzle_3_0) 0))
  :effect (and (assign (puzzle_3_0) (puzzle_3_1)) (assign (puzzle_3_1) 0) (increase (total-cost) 1)))
 (:action move_left_3_2
  :parameters ()
  :precondition (and (= (puzzle_3_1) 0))
  :effect (and (assign (puzzle_3_1) (puzzle_3_2)) (assign (puzzle_3_2) 0) (increase (total-cost) 1)))
 (:action move_left_3_3
  :parameters ()
  :precondition (and (= (puzzle_3_2) 0))
  :effect (and (assign (puzzle_3_2) (puzzle_3_3)) (assign (puzzle_3_3) 0) (increase (total-cost) 1)))
 (:action move_right_0_0
  :parameters ()
  :precondition (and (= (puzzle_0_1) 0))
  :effect (and (assign (puzzle_0_1) (puzzle_0_0)) (assign (puzzle_0_0) 0) (increase (total-cost) 1)))
 (:action move_right_0_1
  :parameters ()
  :precondition (and (= (puzzle_0_2) 0))
  :effect (and (assign (puzzle_0_2) (puzzle_0_1)) (assign (puzzle_0_1) 0) (increase (total-cost) 1)))
 (:action move_right_0_2
  :parameters ()
  :precondition (and (= (puzzle_0_3) 0))
  :effect (and (assign (puzzle_0_3) (puzzle_0_2)) (assign (puzzle_0_2) 0) (increase (total-cost) 1)))
 (:action move_right_1_0
  :parameters ()
  :precondition (and (= (puzzle_1_1) 0))
  :effect (and (assign (puzzle_1_1) (puzzle_1_0)) (assign (puzzle_1_0) 0) (increase (total-cost) 1)))
 (:action move_right_1_1
  :parameters ()
  :precondition (and (= (puzzle_1_2) 0))
  :effect (and (assign (puzzle_1_2) (puzzle_1_1)) (assign (puzzle_1_1) 0) (increase (total-cost) 1)))
 (:action move_right_1_2
  :parameters ()
  :precondition (and (= (puzzle_1_3) 0))
  :effect (and (assign (puzzle_1_3) (puzzle_1_2)) (assign (puzzle_1_2) 0) (increase (total-cost) 1)))
 (:action move_right_2_0
  :parameters ()
  :precondition (and (= (puzzle_2_1) 0))
  :effect (and (assign (puzzle_2_1) (puzzle_2_0)) (assign (puzzle_2_0) 0) (increase (total-cost) 1)))
 (:action move_right_2_1
  :parameters ()
  :precondition (and (= (puzzle_2_2) 0))
  :effect (and (assign (puzzle_2_2) (puzzle_2_1)) (assign (puzzle_2_1) 0) (increase (total-cost) 1)))
 (:action move_right_2_2
  :parameters ()
  :precondition (and (= (puzzle_2_3) 0))
  :effect (and (assign (puzzle_2_3) (puzzle_2_2)) (assign (puzzle_2_2) 0) (increase (total-cost) 1)))
 (:action move_right_3_0
  :parameters ()
  :precondition (and (= (puzzle_3_1) 0))
  :effect (and (assign (puzzle_3_1) (puzzle_3_0)) (assign (puzzle_3_0) 0) (increase (total-cost) 1)))
 (:action move_right_3_1
  :parameters ()
  :precondition (and (= (puzzle_3_2) 0))
  :effect (and (assign (puzzle_3_2) (puzzle_3_1)) (assign (puzzle_3_1) 0) (increase (total-cost) 1)))
 (:action move_right_3_2
  :parameters ()
  :precondition (and (= (puzzle_3_3) 0))
  :effect (and (assign (puzzle_3_3) (puzzle_3_2)) (assign (puzzle_3_2) 0) (increase (total-cost) 1)))
)
