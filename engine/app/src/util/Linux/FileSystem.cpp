#include "stdafx.h"

#include "util/FileSystem.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/system/error_code.hpp>

#include "rbx/Debug.h"
#include "util/standardout.h"

#include <unistd.h>     // readlink
#include <limits.h>     // PATH_MAX
#include <cstdlib>      // getenv

namespace RBX
{
namespace FileSystem
{

boost::filesystem::path getUserDirectory(bool create, FileSystemDir dir, const char* subDirectory)
{
    // ---------------------------- Linux / POSIX implementation ----------------------------
    // Helper: determine executable directory
    auto getExeDir = []() -> boost::filesystem::path
    {
        char buf[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len == -1)
            return "";
        buf[len] = '\0';
        boost::filesystem::path p(buf);
        p.remove_filename();
        return p;
    };

    // Handle DirExe first
    if (dir == DirExe)
    {
        boost::filesystem::path path = getExeDir();
        if (subDirectory)
            path /= subDirectory;

        if (create) {
            boost::system::error_code ec;
            boost::filesystem::create_directories(path, ec);
        }

        return path;
    }

    // Determine HOME directory
    const char* homeEnv = ::getenv("HOME");
    boost::filesystem::path home = homeEnv ? homeEnv : "/tmp";

    boost::filesystem::path base;

    switch (dir)
    {
        case DirAppData:
        {
            const char* xdgDataHome = ::getenv("XDG_DATA_HOME");
            if (xdgDataHome && *xdgDataHome)
                base = xdgDataHome;
            else
                base = home / ".local" / "share";
            break;
        }
        case DirPicture:
            base = home / "Pictures";
            break;
        case DirVideo:
            base = home / "Videos";
            break;
        default:
            RBXASSERT(false);
            return "";
    }

    // Append our application folder and optional subdirectory
    boost::filesystem::path result = base / "ROBLOX";
    if (subDirectory)
        result /= subDirectory;

    if (create)
    {
        boost::system::error_code ec;
        boost::filesystem::create_directories(result, ec);
    }

    return result;
}

boost::filesystem::path getLogsDirectory()
{

    boost::filesystem::path path = getUserDirectory(true, DirAppData, "logs");

    boost::system::error_code ec;
    if (!boost::filesystem::exists(path, ec))
    {
        if (!boost::filesystem::create_directories(path, ec))
        {
            // fallback to cache directory
            return getCacheDirectory(true, "logs");
        }
    }

    return path;
}

} // namespace FileSystem
} // namespace RBX
