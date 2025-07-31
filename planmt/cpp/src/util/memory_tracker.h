#pragma once

#include <string>
#include <chrono>
#include <memory>

namespace planmt {

/**
 * @brief Simple memory tracking utility for monitoring RSS memory usage
 * 
 * Provides lightweight tracking of current and peak memory usage during
 * planning operations. Platform-specific implementations for Linux and macOS.
 */
class MemoryTracker {
public:

    /**
     * @brief Get the singleton instance
     */
    static MemoryTracker& instance();
    
    
    /**
     * @brief Get current memory usage
     * @return Current RSS memory in MB
     */
    double get_current_memory_mb();

private:
    
    MemoryTracker() = default;
    ~MemoryTracker() = default;
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;
    
    /**
     * @brief Platform-specific memory reading implementation
     * @return RSS memory in MB, or 0.0 if unable to read
     */
    double read_rss_memory_mb();
};


} // namespace planmt