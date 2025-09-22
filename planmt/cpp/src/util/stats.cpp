#include "stats.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

namespace planmt {

std::unique_ptr<Stats> Stats::instance_;

Stats& Stats::instance() {
    if (!instance_) {
        instance_ = std::unique_ptr<Stats>(new Stats());
    }
    return *instance_;
}

void Stats::set(const std::string& key, double value) {
    stats_[key] = value;
}

double Stats::get(const std::string& key) const {
    auto it = stats_.find(key);
    return (it != stats_.end()) ? it->second : 0.0;
}

void Stats::add(const std::string& key, double value) {
    stats_[key] += value;  // If key doesn't exist, it starts at 0
}

void Stats::clear() {
    stats_.clear();
}

void Stats::print_all() const {
    if (stats_.empty()) {
        std::cout << "No statistics collected." << std::endl;
        return;
    }
    
    std::cout << "\n=== Statistics ===" << std::endl;
    for (const auto& pair : stats_) {
        std::cout << pair.first << ": " << std::fixed << std::setprecision(3) << pair.second << std::endl;
    }
    std::cout << "==================" << std::endl;
}

void Stats::write_to_file(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Error: Could not open stats file for writing: " << filename << std::endl;
        return;
    }
    
    for (const auto& pair : stats_) {
        file << pair.first << ": " << std::fixed << std::setprecision(3) << pair.second << std::endl;
    }
    
    file.close();
    std::cout << "Statistics written to: " << filename << std::endl;
}

size_t Stats::size() const {
    return stats_.size();
}

} // namespace planmt