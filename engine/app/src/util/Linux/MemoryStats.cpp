#if defined(__linux__) // Guard for Linux-specific implementation

#include "util/MemoryStats.h"
#include <fstream>
#include <string>
#include <sstream>
#include <cstdint> // For uint64_t, which is a safer replacement for DWORDLONG

// Define the RBX namespace and sub-namespaces if they are not already.
// This ensures the code can be compiled standalone for testing.
#ifndef RBX
namespace RBX {
    namespace MemoryStats {
        // Forward declarations from a potential header file
        uint64_t usedMemoryBytes();
        uint64_t freeMemoryBytes();
        uint64_t totalMemoryBytes();
    }
}
#endif

namespace RBX {
namespace MemoryStats {

/**
 * @brief Parses a /proc file to find a specific key and returns its value.
 *
 * This is a helper function to read files like /proc/meminfo and /proc/self/status.
 * It searches for a line starting with the given key (e.g., "MemTotal:") and
 * extracts the numerical value that follows.
 *
 * @param filePath The full path to the file to parse.
 * @param key The key to search for within the file (e.g., "MemTotal:").
 * @return The numerical value associated with the key, converted to bytes.
 * Returns 0 if the key is not found or an error occurs.
 */
static uint64_t getValueFromProcFile(const std::string& filePath, const std::string& key) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // Failed to open the file, cannot retrieve stats.
        return 0;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind(key, 0) == 0) { // Check if the line starts with the key
            std::stringstream ss(line.substr(key.length()));
            uint64_t value = 0;
            ss >> value;
            // Values in /proc files are typically in kB, so convert to bytes.
            return value * 1024;
        }
    }

    return 0; // Key not found
}

/**
 * @brief Gets the current memory usage of this process (Resident Set Size).
 *
 * This is the Linux equivalent of GetProcessMemoryInfo's WorkingSetSize. It reads
 * the "VmRSS" value from /proc/self/status, which represents the portion of the
 * process's memory currently held in RAM.
 *
 * @return The used physical memory by the current process in bytes.
 */
uint64_t usedMemoryBytes() {
    // "VmRSS" is the Resident Set Size and is a good measure for the working set.
    return getValueFromProcFile("/proc/self/status", "VmRSS:");
}

/**
 * @brief Gets the amount of free physical memory available in the system.
 *
 * This function reads the "MemAvailable" value from /proc/meminfo. "MemAvailable" is
 * a more accurate estimation of how much memory is available for new applications
 * than "MemFree".
 *
 * @return The total available physical memory in the system in bytes.
 */
uint64_t freeMemoryBytes() {
    // "MemAvailable" is a more realistic measure of available memory.
    return getValueFromProcFile("/proc/meminfo", "MemAvailable:");
}

/**
 * @brief Gets the total physical memory (RAM) in the system.
 *
 * This function reads the "MemTotal" value from the /proc/meminfo file.
 *
 * @return The total physical memory in the system in bytes.
 */
uint64_t totalMemoryBytes() {
    return getValueFromProcFile("/proc/meminfo", "MemTotal:");
}

} // namespace MemoryStats
} // namespace RBX

#endif // defined(__linux__)
