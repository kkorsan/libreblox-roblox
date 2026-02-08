#include "OperationalSecurity.h"
#include "stdafx.h" // Standard precompiled header include

#include <boost/smart_ptr/scoped_ptr.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstring>
#include <thread>
#include <chrono> // For std::chrono::milliseconds
#include <memory>
#include <mutex>
#include <csignal> // For signal and SIGINT
#include <unistd.h>
#include <cstdlib> // For getenv



#include "nlohmann/json.hpp" // For JSON handling
#include "util/Statistics.h"
#include "util/standardout.h"
#include "util/Http.h"

// Global atomic flag for graceful shutdown
static std::atomic<bool> s_shuttingDown(false);

// Signal handler to set shutdown flag
static void handleSignal(int)
{
    s_shuttingDown = true;
}

// Global atomic counter for requests (from old SOAP version)
std::atomic<long> requestCount(0);

// Self-pipe for interrupting the main loop on signal
static int self_pipe[2];

// Forward declarations for external functions from RCCServiceHttpImpl.cpp
// These functions are assumed to be defined in RCCServiceHttpImpl.cpp
void start_CWebService(const char* contentPath, bool crashOnFail);
void stop_CWebService();

// Forward declarations for structs from RCCServiceHttpImpl.cpp
struct Job
{
    std::string id;
    double expirationInSeconds;
    int category;
    double cores;
};

struct ScriptExecution
{
    std::string name;
    std::string script;
    std::vector<nlohmann::json> arguments;
};

// Forward declaration of CWebService class and its static singleton
class CWebService
{
public:
    static boost::scoped_ptr<CWebService> singleton;
    // Declare the method that will be called directly
    void openJobAndExecuteScript(const Job& job, const ScriptExecution& script, std::vector<nlohmann::json>& result, bool startHeartbeat);
    // Declare server start/stop methods
    void startServer(int port);
    void stopServer();
    void closeAllJobs(int* result);
    bool isServerStopped();
    std::atomic<bool> serverStopped;
};

class PrintfLoggerLinux
{
    rbx::signals::scoped_connection messageConnection;
    std::mutex mutex;
public:
    PrintfLoggerLinux()
    {
        messageConnection = RBX::StandardOut::singleton()->messageOut.connect(boost::bind(&PrintfLoggerLinux::onMessage, this, _1));
    }
protected:
    void onMessage(const RBX::StandardOutMessage& message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        switch (message.type)
        {
        case RBX::MESSAGE_OUTPUT:
        case RBX::MESSAGE_INFO:
            std::cout << message.message << std::endl;
            break;
        case RBX::MESSAGE_WARNING:
        case RBX::MESSAGE_ERROR:
            std::cerr << message.message << std::endl;
            break;
        }
    }
};

// Command line parsing functions (unchanged from user's provided code)
static int parsePort(int argc, char* argv[])
{
    int port = 64989;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '/' || argv[i][0] == '-')
            continue;
        port = atoi(argv[i]);
    }
    return port;
}

static const char* parseContent(int argc, char* argv[])
{
    const char* szTag = "-Content:";
    int tagLen = strlen(szTag);
    static const char* szDefaultValue = "content/";

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '/')
            argv[i][0] = '-';

        if (strncmp(argv[i], szTag, tagLen) == 0)
        {
            return argv[i] + tagLen;
        }
    }
    return szDefaultValue;
}

static int parsePlaceId(int argc, char* argv[])
{
    const char* szTag = "-PlaceId:";
    int tagLen = strlen(szTag);

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '/')
            argv[i][0] = '-';

        if (strncmp(argv[i], szTag, tagLen) == 0)
        {
            return atoi(argv[i] + tagLen);
        }
    }
    return -1;
}

static bool parseConsole(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '/')
            argv[i][0] = '-';

        if (strcmp(argv[i], "-Console") == 0)
        {
            return true;
        }
    }
    return false;
}

// Startup, step, and shutdown functions for RCCService (unchanged from user's provided code)
void startupRCC(int port, const char* contentPath, bool crashOnFail)
{
    std::cout << "RCCService starting on port " << port << std::endl;
    start_CWebService(contentPath, crashOnFail);
    CWebService::singleton->startServer(port);
}

void stepRCC()
{
    // Check if server has stopped (e.g., due to Ctrl-C) and set shutdown flag immediately
    if (CWebService::singleton && CWebService::singleton->isServerStopped()) {
        s_shuttingDown = true;
        return;
    }

    // Use short sleeps with frequent checks for shutdown signal
    for (int i = 0; i < 100 && !s_shuttingDown; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void shutdownRCC()
{
    std::cerr << "RCCService shutting down" << std::endl;
    int result;
    CWebService::singleton->closeAllJobs(&result);
    if (!CWebService::singleton->isServerStopped()) {
        CWebService::singleton->stopServer();
    }
    stop_CWebService();
}

// Linux-friendly ReadAccessKey implementation: read from environment variable RBX_HTTP_ACCESS_KEY
// This avoids dependency on Windows registry on Linux, and makes it easy to provide the access
// key when running under Xvfb or on headless servers.
void ReadAccessKey()
{
    if (RBX::Http::accessKey.empty())
    {
        const char* env = std::getenv("RBX_HTTP_ACCESS_KEY");
        if (env && env[0] != '\0')
        {
            RBX::Http::accessKey = std::string(env);
            RBX::StandardOut::singleton()->printf(RBX::MESSAGE_SENSITIVE, "Access key read from env: %s", RBX::Http::accessKey.c_str());
        }
        else
        {
            RBX::StandardOut::singleton()->print(RBX::MESSAGE_SENSITIVE, "No access key found in RBX_HTTP_ACCESS_KEY");
        }
    }

    RBX::StandardOut::singleton()->printf(RBX::MESSAGE_SENSITIVE, "Current Access key: %s", RBX::Http::accessKey.c_str());
}



int main(int argc, char* argv[])
{
    static std::unique_ptr<PrintfLoggerLinux> standardOutLog(new PrintfLoggerLinux());

    // read access key
    ReadAccessKey();

    bool isConsole = parseConsole(argc, argv);

    if (isConsole)
    {
        int port = parsePort(argc, argv);
        const char* contentPath = parseContent(argc, argv);
        bool crashOnFail = false; // Assuming false for console mode

        startupRCC(port, contentPath, crashOnFail);

        // Check if SIGINT is blocked
        sigset_t sigset;
        sigprocmask(0, NULL, &sigset);
        if (sigismember(&sigset, SIGINT)) {
            sigset_t newset;
            sigemptyset(&newset);
            sigaddset(&newset, SIGINT);
            sigprocmask(SIG_UNBLOCK, &newset, NULL);
        }

        // Reinstall signal handler after server startup to override any Crow-installed handler
        struct sigaction sa;
        sa.sa_handler = handleSignal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("sigaction");
            return 1;
        }

        int placeId = parsePlaceId(argc, argv);
        if (placeId > -1)
        {
            std::cout << "Starting game server for PlaceId: " << placeId << std::endl;

            // Read gameserver.txt and create startup script
            std::stringstream buffer;
            std::ifstream gameServerFile("gameserver.txt", std::ios::in);
            if (gameServerFile.is_open())
            {
                buffer << gameServerFile.rdbuf();
                gameServerFile.close();
            } else {
                std::cerr << "Warning: gameserver.txt not found. Using default script." << std::endl;
            }

            buffer << "start(" << placeId << ", " << 53640 << ", '" << GetBaseURL() << "')";

            std::string script = buffer.str();

            try {
                // Call OpenJob function directly via CWebService singleton
                Job job;
                job.id = "Test";
                job.expirationInSeconds = 600;
                job.category = 0;
                job.cores = 1.0;

                ScriptExecution se;
                se.name = "StartServer";
                se.script = script;

                std::vector<nlohmann::json> result_json;
                CWebService::singleton->openJobAndExecuteScript(job, se, result_json, true);
                std::cout << "Game server startup initiated successfully" << std::endl;
            }
            catch (const std::exception& e) {
                std::cout << "Failed to start game server: " << e.what() << std::endl;
            }
        }

        while (!s_shuttingDown)
        {
            stepRCC();
        }

        shutdownRCC();
    }
    else
    {
        std::cout << "Use -Console argument to run in console mode" << std::endl;
    }

    // Clean up Lua read-only protection
    RBX::clearLuaReadOnly();

    return 0;
}
