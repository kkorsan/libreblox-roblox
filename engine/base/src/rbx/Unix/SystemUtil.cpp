#include "rbx/SystemUtil.h"
#include "util/StreamHelpers.h"

#include <fstream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <GL/gl.h>
#include <cstring>
#include <set>
#include <dirent.h>

using namespace RBX;

namespace RBX
{
namespace SystemUtil
{

std::string getCPUMake()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;

    while (std::getline(cpuinfo, line)) {
        if (line.find("vendor_id") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string vendor = line.substr(pos + 1);
                // Trim whitespace
                vendor.erase(0, vendor.find_first_not_of(" \t"));
                vendor.erase(vendor.find_last_not_of(" \t") + 1);

                if (vendor.find("GenuineIntel") != std::string::npos) {
                    return "Intel";
                } else if (vendor.find("AuthenticAMD") != std::string::npos) {
                    return "AMD";
                } else if (vendor.find("ARM") != std::string::npos) {
                    return "ARM";
                }
                return vendor;
            }
        }
    }

#ifdef __x86_64__
    return "x86_64";
#elif __i386__
    return "x86";
#elif __arm__
    return "ARM";
#elif __aarch64__
    return "ARM64";
#else
    return "Unknown";
#endif
}

uint64_t getCPUSpeed()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;

    while (std::getline(cpuinfo, line)) {
        if (line.find("cpu MHz") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string speed = line.substr(pos + 1);
                speed.erase(0, speed.find_first_not_of(" \t"));
                try {
                    return static_cast<uint64_t>(std::stod(speed) * 1000000); // Convert MHz to Hz
                } catch (...) {
                    break;
                }
            }
        }
    }
    return 0;
}

uint64_t getCPULogicalCount()
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

uint64_t getCPUCoreCount()
{
    return sysconf(_SC_NPROCESSORS_CONF);
}

uint64_t getCPUPhysicalCount()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::set<std::string> physicalIds;

    while (std::getline(cpuinfo, line)) {
        if (line.find("physical id") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string id = line.substr(pos + 1);
                id.erase(0, id.find_first_not_of(" \t"));
                id.erase(id.find_last_not_of(" \t") + 1);
                physicalIds.insert(id);
            }
        }
    }

    return physicalIds.empty() ? 1 : physicalIds.size();
}

bool isCPU64Bit()
{
#ifdef __x86_64__
    return true;
#elif __aarch64__
    return true;
#else
    return false;
#endif
}

uint64_t getMBSysRAM()
{
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (info.totalram * info.mem_unit) / (1024 * 1024);
    }
    return 0;
}

uint64_t getMBSysAvailableRAM()
{
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (info.freeram * info.mem_unit) / (1024 * 1024);
    }
    return 0;
}

uint64_t getVideoMemory()
{
    // 1. Try NVIDIA /proc interface (Reliable, no context needed)
    DIR* dir = opendir("/proc/driver/nvidia/gpus");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            
            std::string infoPath = std::string("/proc/driver/nvidia/gpus/") + entry->d_name + "/information";
            std::ifstream infoFile(infoPath.c_str());
            std::string line;
            while (std::getline(infoFile, line)) {
                if (line.find("Video RAM") != std::string::npos) {
                    size_t colon = line.find(":");
                    if (colon != std::string::npos) {
                        std::string valStr = line.substr(colon + 1);
                        std::stringstream ss(valStr);
                        uint64_t val;
                        std::string unit;
                        if (ss >> val >> unit) {
                            uint64_t multiplier = 1024 * 1024;
                            if (unit == "KB" || unit == "kB") multiplier = 1024;
                            else if (unit == "MB") multiplier = 1024 * 1024;
                            else if (unit == "GB") multiplier = 1024 * 1024 * 1024ULL;
                            
                            closedir(dir);
                            return val * multiplier;
                        }
                    }
                }
            }
        }
        closedir(dir);
    }

    // 2. Try OpenGL extensions (NVIDIA)
    // GL_NVX_gpu_memory_info
    #ifndef GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
    #define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
    #endif

    GLint dedicatedVideoMemoryKB = 0;
    while(glGetError() != GL_NO_ERROR); // Clear errors
    glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicatedVideoMemoryKB);
    
    if (glGetError() == GL_NO_ERROR && dedicatedVideoMemoryKB > 0) {
        return static_cast<uint64_t>(dedicatedVideoMemoryKB) * 1024;
    }

    // Fallback: estimate based on system RAM
    // VmallocTotal is unreliable for VRAM.
    uint64_t totalRam = getMBSysRAM();
    if (totalRam > 16384) { // > 16GB
        return 4096 * 1024 * 1024ULL; // 4GB assumption for high RAM systems
    } else if (totalRam > 8192) { // > 8GB
        return 2048 * 1024 * 1024ULL; // 2GB
    } else if (totalRam > 4096) { // > 4GB
        return 1024 * 1024 * 1024ULL; // 1GB
    } else {
        return 512 * 1024 * 1024ULL; // 512MB
    }
}

std::string osPlatform()
{
    return "Linux";
}

int osPlatformId()
{
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        std::string release = unameData.release;
        // Try to extract major version number
        size_t dotPos = release.find('.');
        if (dotPos != std::string::npos) {
            try {
                return std::stoi(release.substr(0, dotPos));
            } catch (...) {
                // Fall through to default
            }
        }
    }
    return 1; // Default version
}

std::string osVer()
{
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        std::stringstream ss;
        ss << unameData.sysname << " " << unameData.release;
        return ss.str();
    }
    return "Linux Unknown";
}

std::string deviceName()
{
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        return std::string(unameData.nodename);
    }
    return "Linux Computer";
}

std::string getGPUMake()
{
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    if (vendor && renderer) {
        std::string result = std::string(vendor) + " " + std::string(renderer);
        return result;
    } else if (vendor) {
        return std::string(vendor);
    } else if (renderer) {
        return std::string(renderer);
    }

    // Fallback: try to detect from lspci or other sources
    return "Unknown GPU";
}

std::string getMaxRes()
{
    // This would typically require X11 or Wayland calls to get display info
    // For now, return empty string as fallback
    return "";
}

} // SystemUtil
} // RBX
