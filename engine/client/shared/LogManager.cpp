#include "stdafx.h"
#include "LogManager.h"
#include "RbxFormat.h"
#include "rbx/Debug.h"
#include "rbx/Boost.hpp"
#include "util/standardout.h"
#include "util/FileSystem.h"
#include "util/Guid.h"
#include "util/Http.h"
#include "util/Statistics.h"
#include "g3d/debugAssert.h"
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include "VersionInfo.h"
#include "rbx/TaskScheduler.h"
#include "DumpErrorUploader.h"
#include "rbx/Log.h"
#include "FastLog.h"

#ifdef __linux__
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstring>
#endif

LOGGROUP(CrashReporterInit)

bool LogManager::logsEnabled = true;
MainLogManager* LogManager::mainLogManager = NULL;
RBX::mutex MainLogManager::fastLogChannelsLock;
rbx::atomic<int> LogManager::nextThreadID = 0;

static const boost::filesystem::path& DoGetPath()
{
    static boost::filesystem::path path = RBX::FileSystem::getLogsDirectory();
    return path;
}

void InitPath()
{
    DoGetPath();
}

std::string GetAppVersion()
{
    CVersionInfo vi;
    vi.Load();
    return vi.GetFileVersionAsString();
}

const boost::filesystem::path& LogManager::GetLogPath() const
{
    static boost::once_flag flag = BOOST_ONCE_INIT;
    boost::call_once(&InitPath, flag);
    return DoGetPath();
}

void MainLogManager::fastLogMessage(FLog::Channel id, const char* message)
{
    RBX::mutex::scoped_lock lock(fastLogChannelsLock);
    if(mainLogManager)
    {
        if(id >= mainLogManager->fastLogChannels.size())
            mainLogManager->fastLogChannels.resize(id+1, NULL);
        if(mainLogManager->fastLogChannels[id] == NULL)
        {
            mainLogManager->fastLogChannels[id] = new RBX::Log(mainLogManager->getFastLogFileName(id).c_str(), "Log Channel");
        }
        mainLogManager->fastLogChannels[id]->writeEntry(RBX::Log::Information, message);
    }
}

std::string MainLogManager::getSessionId()
{
    std::string id = guid;
    id.erase(8);
    id.erase(0,3);
    return id;
}

std::string MainLogManager::getCrashEventName()
{
    FASTLOG(FLog::CrashReporterInit, "Getting crash event name");
    boost::filesystem::path path = GetLogPath();
    std::string fileName = "log_";
    fileName += getSessionId();
    fileName += " ";
    fileName += GetAppVersion();
    fileName += crashEventExtention;
    path /= fileName;
    return path.string();
}

std::string MainLogManager::getLogFileName()
{
    boost::filesystem::path path = GetLogPath();
    std::string fileName = "log_";
    fileName += getSessionId();
    fileName += ".txt";
    path /= fileName;
    return path.string();
}

std::string MainLogManager::getFastLogFileName(FLog::Channel channelId)
{
    boost::filesystem::path path = GetLogPath();
    std::string filename = RBX::format("log_%s_%d.txt", getSessionId().c_str(), channelId);
    path /= filename;
    return path.string();
}

std::string MainLogManager::MakeLogFileName(const char* postfix)
{
    boost::filesystem::path path = GetLogPath();
    std::string fileName = "log_";
    fileName += getSessionId();
    fileName += postfix;
    fileName += ".txt";
    path /= fileName;
    return path.string();
}

std::string ThreadLogManager::getLogFileName()
{
    std::string fileName = mainLogManager->getLogFileName();
    std::string id = RBX::format("_%s_%d", name.c_str(), threadID);
    fileName.insert(fileName.size()-4, id);
    return fileName;
}

RBX::Log* LogManager::getLog()
{
    if (!logsEnabled)
        return NULL;
    if (log==NULL)
    {
        log = new RBX::Log(getLogFileName().c_str(), name.c_str());
    }
    return log;
}

RBX::Log* MainLogManager::provideLog()
{
    if (boost::this_thread::get_id() == boost::thread::id())
        return this->getLog();
    return ThreadLogManager::getCurrent()->getLog();
}

#ifdef __linux__
static void signalHandler(int sig, siginfo_t* info, void* context)
{
    // Log the crash
    LogManager::ReportEvent(EVENTLOG_ERROR_TYPE, RBX::format("Signal %d received", sig).c_str());

    // Generate backtrace
    void* array[10];
    size_t size = backtrace(array, 10);
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    // Upload crash event
    DumpErrorUploader::UploadCrashEventFile(NULL); // No EXCEPTION_POINTERS on Linux

    // Exit
    _exit(1);
}
#endif

RobloxCrashReporter::RobloxCrashReporter(const char* outputPath, const char* appName, const char* crashExtention)
{
#ifdef __linux__
    // Set up signal handlers for crashes
    struct sigaction sa;
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
#endif
}

bool RobloxCrashReporter::silent;

LONG RobloxCrashReporter::ProcessException(struct _EXCEPTION_POINTERS *info, bool noMsg)
{
    LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, "StartProcessException...");
    LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, "DoneProcessException");
    LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, "Uploading .crashevent...");
    DumpErrorUploader::UploadCrashEventFile(info);
    LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, "Done uploading .crashevent...");
    return 0;
}

void RobloxCrashReporter::logEvent(const char* msg)
{
    LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, msg);
}

void MainLogManager::WriteCrashDump()
{
    std::string appName = "log_";
    appName += getSessionId();
    crashReporter.reset(new RobloxCrashReporter(GetLogPath().string().c_str(), appName.c_str(), crashExtention));
}

bool MainLogManager::CreateFakeCrashDump()
{
    // Create a fake core dump file
    boost::filesystem::path path = GetLogPath();
    std::string fileName = "log_";
    fileName += getSessionId();
    fileName += ".dmp";
    path /= fileName;

    std::ofstream file(path.string().c_str(), std::ios::out | std::ios::binary);
    if (file.is_open()) {
        file << "Fake crash dump";
        file.close();
        return true;
    }
    return false;
}

void MainLogManager::EnableImmediateCrashUpload(bool enabled)
{
    // Stub for Linux
}

void MainLogManager::DisableHangReporting()
{
    // Stub for Linux
}

void MainLogManager::NotifyFGThreadAlive()
{
    // Stub for Linux
}

static void purecallHandler(void)
{
    LogManager::ReportEvent(EVENTLOG_ERROR_TYPE, "Pure Call Error");
    RBXCRASH();
}

MainLogManager::MainLogManager(const char* productName, const char* crashExtention, const char* crashEventExtention)
    :LogManager(productName),
    crashExtention(crashExtention),
    crashEventExtention(crashEventExtention),
    gameState(MainLogManager::GameState::UN_INITIALIZED)
{
    RBX::Guid::generateRBXGUID(guid);
    CullLogs("", 1024);
    CullLogs("archive/", 1024);
    mainLogManager = this;
    RBX::Log::setLogProvider(this);
    RBX::setAssertionHook(&MainLogManager::handleDebugAssert);
    RBX::setFailureHook(&MainLogManager::handleFailure);
    FLog::SetExternalLogFunc(fastLogMessage);
}

MainLogManager* LogManager::getMainLogManager() {
    return mainLogManager;
}

ThreadLogManager::ThreadLogManager()
    :LogManager(RBX::get_thread_name())
{
}

ThreadLogManager::~ThreadLogManager()
{
}

std::vector<std::string> MainLogManager::gatherScriptCrashLogs()
{
    std::vector<std::string> result;
    boost::filesystem::path path = GetLogPath();
    path /= "";

    if (boost::filesystem::exists(path) && boost::filesystem::is_directory(path)) {
        for (boost::filesystem::directory_iterator it(path); it != boost::filesystem::directory_iterator(); ++it) {
            if (boost::filesystem::is_regular_file(it->path()) && it->path().extension() == ".cse") {
                result.push_back(it->path().string());
            }
        }
    }
    return result;
}

std::vector<std::string> MainLogManager::gatherCrashLogs()
{
    std::vector<std::string> result;
    boost::filesystem::path path = GetLogPath();
    path /= "";

    if (boost::filesystem::exists(path) && boost::filesystem::is_directory(path)) {
        for (boost::filesystem::directory_iterator it(path); it != boost::filesystem::directory_iterator(); ++it) {
            if (boost::filesystem::is_regular_file(it->path()) && it->path().extension() == ".dmp") {
                result.push_back(it->path().string());
                // Gather associated logs
                std::string pattern = it->path().stem().string() + "*.*";
                std::vector<std::string> logs = gatherAssociatedLogs(pattern);
                result.insert(result.end(), logs.begin(), logs.end());
            }
        }
    }
    return result;
}

std::vector<std::string> MainLogManager::gatherAssociatedLogs(const std::string& filenamepattern)
{
    std::vector<std::string> result;
    boost::filesystem::path path = GetLogPath();
    path /= "";

    if (boost::filesystem::exists(path) && boost::filesystem::is_directory(path)) {
        for (boost::filesystem::directory_iterator it(path); it != boost::filesystem::directory_iterator(); ++it) {
            if (boost::filesystem::is_regular_file(it->path())) {
                std::string filename = it->path().filename().string();
                if (filename.find(filenamepattern.substr(0, 9)) != std::string::npos && filename != filenamepattern) {
                    result.push_back(it->path().string());
                }
            }
        }
    }
    return result;
}

bool MainLogManager::hasCrashLogs(std::string extension) const
{
    boost::filesystem::path path = GetLogPath();
    path /= "";

    if (boost::filesystem::exists(path) && boost::filesystem::is_directory(path)) {
        for (boost::filesystem::directory_iterator it(path); it != boost::filesystem::directory_iterator(); ++it) {
            if (boost::filesystem::is_regular_file(it->path()) && it->path().extension() == extension) {
                return true;
            }
        }
    }
    return false;
}

void MainLogManager::CullLogs(const char* folder, int filesRemaining)
{
    boost::filesystem::path path = GetLogPath();
    path /= folder;

    if (!boost::filesystem::exists(path) || !boost::filesystem::is_directory(path)) return;

    std::vector<boost::filesystem::path> files;
    for (boost::filesystem::directory_iterator it(path); it != boost::filesystem::directory_iterator(); ++it) {
        if (boost::filesystem::is_regular_file(it->path())) {
            files.push_back(it->path());
        }
    }

    // Sort by modification time, oldest first
    std::sort(files.begin(), files.end(), [](const boost::filesystem::path& a, const boost::filesystem::path& b) {
        return boost::filesystem::last_write_time(a) < boost::filesystem::last_write_time(b);
    });

    for (size_t i = 0; i + filesRemaining < files.size(); ++i) {
        boost::filesystem::remove(files[i]);
    }
}

MainLogManager::~MainLogManager()
{
    RBX::Log::setLogProvider(NULL);
    RBX::mutex::scoped_lock lock(fastLogChannelsLock);
    FLog::SetExternalLogFunc(NULL);
    for(std::size_t i = 0; i < fastLogChannels.size(); i++)
        delete fastLogChannels[i];
    mainLogManager = NULL;
}

LogManager::~LogManager()
{
    if (log != NULL)
    {
        std::string logFile = log->logFile;
        delete log;
        log = NULL;
    }
}

bool MainLogManager::handleG3DDebugAssert(
    const char* _expression,
    const std::string& message,
    const char* filename,
    int lineNumber,
    bool useGuiPrompt)
{
    return handleDebugAssert(_expression,filename,lineNumber);
}

bool MainLogManager::handleDebugAssert(
    const char* expression,
    const char* filename,
    int lineNumber
)
{
    std::string eventMessage = RBX::format("Assertion failed: %s\n%s(%d)", expression, filename, lineNumber);
    LogManager::ReportEvent(EVENTLOG_WARNING_TYPE, eventMessage.c_str());
    //RBXCRASH(); WHY CRASH??????
    return true;
}

bool MainLogManager::handleG3DFailure(
    const char* _expression,
    const std::string& message,
    const char* filename,
    int lineNumber,
    bool useGuiPrompt)
{
    return handleFailure(_expression, filename, lineNumber);
}

bool MainLogManager::handleFailure(
    const char* expression,
    const char* filename,
    int lineNumber
)
{
    std::string eventMessage = RBX::format("G3D Error: %s\n%s(%d)", expression, filename, lineNumber);
    LogManager::ReportEvent(EVENTLOG_ERROR_TYPE, eventMessage.c_str());
    RBXCRASH();
    return false;
}

void LogManager::ReportException(std::exception const& exp)
{
    RBX::StandardOut::singleton()->print(RBX::MESSAGE_ERROR, exp);
}

void LogManager::ReportLastError(const char* message)
{
    RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "%s, errno=%d", message, errno);
}

void LogManager::ReportEvent(int type, const char* message)
{
    switch (type)
    {
    case EVENTLOG_SUCCESS:
        RBX::StandardOut::singleton()->printf(RBX::MESSAGE_INFO, "%s", message);
        break;
    case EVENTLOG_ERROR_TYPE:
        RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "%s", message);
        break;
    case EVENTLOG_INFORMATION_TYPE:
        RBX::StandardOut::singleton()->printf(RBX::MESSAGE_INFO, "%s", message);
        break;
    case EVENTLOG_AUDIT_SUCCESS:
        RBX::StandardOut::singleton()->printf(RBX::MESSAGE_INFO, "%s", message);
        break;
    case EVENTLOG_AUDIT_FAILURE:
        RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "%s", message);
        break;
    }
}

void LogManager::ReportEvent(int type, const char* message, const char* fileName, int lineNumber)
{
    std::string m = RBX::format("%s\n%s(%d)", message, fileName, lineNumber);
    LogManager::ReportEvent(type, m.c_str());
}

namespace log_detail
{
    boost::once_flag once_init = BOOST_ONCE_INIT;
    static boost::thread_specific_ptr<ThreadLogManager>* ts;
    void init(void)
    {
        static boost::thread_specific_ptr<ThreadLogManager> value;
        ts = &value;
    }
}

ThreadLogManager* ThreadLogManager::getCurrent()
{
    boost::call_once(log_detail::init, log_detail::once_init);
    ThreadLogManager* logManager = log_detail::ts->get();
    if (!logManager)
    {
        logManager = new ThreadLogManager();
        log_detail::ts->reset(logManager);
    }
    return logManager;
}

LogManager::LogManager(const char* name)
    : log(NULL), name(name), threadID(++nextThreadID)
{
}
