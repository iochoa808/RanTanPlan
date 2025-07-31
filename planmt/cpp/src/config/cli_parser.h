#pragma once

#include "config.h"

namespace planmt {

class CLIParser {
public:
    void parse(Config& config, int argc, char* argv[]);

private:
    VerbosityLevel parse_verbosity(const std::string& value) const;
};

} // namespace planmt