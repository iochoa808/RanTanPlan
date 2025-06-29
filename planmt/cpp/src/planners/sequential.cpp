#include "sequential.h"

namespace planmt {

    SequentialPlanner::SequentialPlanner(const Problem& problem, GroundedEncoder&
    encoder) : problem_(problem), encoder_(encoder) {
    // Initialize planner with the given problem and encoder
    }

    void SequentialPlanner::search() { 
        std::cout << "initial state:" << *encoder_.encode_initial_state() << std::endl;
        std::cout << "goal:" << *encoder_.encode_goal(1) << std::endl;
        std::cout << "actions:" << *encoder_.encode_actions(1) << std::endl;
        std::cout << "frames:" << *encoder_.encode_frames(1) << std::endl;
        std::cout << "parallelism:" << *encoder_.encode_parallelism(1) << std::endl;
    }

}
