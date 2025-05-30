#pragma once

#include "../problem/problem.h"
#include "../encoders/grounded_encoder.h"
#include <z3++.h>

// This class is able to search for a plan in a sequential manner by using an encoder.
namespace planmt {

class SequentialPlanner {
public:
    // Constructor
    SequentialPlanner(const Problem& problem, GroundedEncoder& encoder);

    // TODO - returns a plan
    void search();

private:
    const Problem& problem_;
    GroundedEncoder& encoder_;
};

} // namespace planmt
