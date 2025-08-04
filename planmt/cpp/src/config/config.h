#pragma once

#include <string>
#include <mutex>
#include <memory>

namespace planmt {

enum class VerbosityLevel {
    SILENT = 0,
    INFO = 1, 
    VERBOSE = 2,
    DEBUG = 3
};

class Config {
public:
    static Config& instance();
    
    struct Global {
        VerbosityLevel verbosity = VerbosityLevel::INFO;
        bool debug_mode = false;
        int timeout = 3600;
        std::string log_level = "INFO";
    } global;
    
    struct Planner {
        std::string strategy = "sequential";
        int max_steps = 100;
    } planner;
    
    struct Propagators {
        std::string type = "null";
        bool persist_clauses = true;
    } propagators;
    
    struct InterferenceAnalyzer {
        bool lazy_computation = false;  // false = eager (default), true = lazy
    } interference_analyzer;
    
    struct Symmetry {
        bool detect_symmetries = false;  // Enable symmetry detection
    } symmetry;
    
    void initialize(int argc, char* argv[]);
    void validate() const;
    
    bool is_silent() const { return global.verbosity == VerbosityLevel::SILENT; }
    bool is_info() const { return global.verbosity >= VerbosityLevel::INFO; }
    bool is_verbose() const { return global.verbosity >= VerbosityLevel::VERBOSE; }
    bool is_debug() const { return global.verbosity >= VerbosityLevel::DEBUG; }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    static std::once_flag initialized_flag_;
    static std::unique_ptr<Config> instance_;
    
    friend std::default_delete<Config>;
};

} // namespace planmt