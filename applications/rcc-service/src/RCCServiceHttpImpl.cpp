#include "CProcessPerfCounter.h"
#include "stdafx.h"

#include "CrossPlatformEvent.h"

#include <crow.h>
#include <nlohmann/json.hpp>

#include "rbx/Debug.h"
#include "util/ProtectedString.h"
#include "util/standardout.h"
#include "boost/noncopyable.hpp"
#include "boost/thread/xtime.hpp"
#include "boost/property_tree/json_parser.hpp"
#include "boost/foreach.hpp"
#include <boost/filesystem.hpp>
#include <atomic>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "rbx/rbxTime.h"
#include "util/standardout.h"
#include "util/Statistics.h"
#include "rbx/TaskScheduler.h"
#include "util/rbxrandom.h"
#include "network/Players.h"

#include "v8datamodel/Workspace.h"
#include "v8datamodel/ContentProvider.h"
#include "v8datamodel/DataModel.h"
#include "v8datamodel/DebugSettings.h"
#include "v8datamodel/PhysicsSettings.h"
#include "v8datamodel/FastLogSettings.h"
#include "script/LuaSettings.h"
#include "v8datamodel/GameSettings.h"
#include "v8datamodel/MarketplaceService.h"
#include "v8datamodel/AdService.h"
#include "v8datamodel/CSGDictionaryService.h"
#include "v8datamodel/NonReplicatedCSGDictionaryService.h"
#include "v8datamodel/Message.h"
#include "RobloxServicesTools.h" // Assuming this header provides GetBaseURL, GetSecurityKeyUrl, GetMD5HashUrl, GetMemHashUrl, FetchClientSettingsData, LoadClientSettingsFromString
#include "script/ScriptContext.h"
#include "thumbnailgenerator.h"
#include <string>
#include <sstream>
#include "v8datamodel/factoryregistration.h"
#include "thumbnailgenerator.h"
#include "util/Profiling.h"
#include "util/SoundService.h"
#include "util/Guid.h"
#include "util/Http.h"
#include "util/Statistics.h"
#include "util/rbxrandom.h"
#include "network/api.h"
#include "VersionInfo.h"
#include "LogManager.h"
#include <queue>

#include "DumpErrorUploader.h"
#include "gui/ProfanityFilter.h"
#include "GfxBase/ViewBase.h"
#include "util/FileSystem.h"
#include "network/Players.h"
#include "v8xml/XmlSerializer.h"
#include "v8xml/WebParser.h"
#include "RobloxServicesTools.h"
#include "util/Utilities.h"
#include "network/ChatFilter.h"
#include "util/RobloxGoogleAnalytics.h"
#include "network/WebChatFilter.h"

#include "v8datamodel/Explosion.h"
#include "v8kernel/Cofm.h"
#include "v8kernel/SimBody.h"
#include "v8kernel/Body.h"
#include "v8kernel/ContactConnector.h"
#include "v8world/Contact.h"
#include "v8xml/XmlElement.h"

#include "CountersClient.h"

#include "SimpleJSON.h" // Assuming this is available for parsing simple JSON
#include "RbxFormat.h"

#include "ProcessInformation.h" // Assuming this is available for process info
#include "util/Analytics.h"
#include "OperationalSecurity.h"

#define BOOST_DATE_TIME_NO_LIB
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp> // For lexical_cast

#include <future> // For std::async and std::future
#include <chrono> // For std::chrono

std::atomic<long> diagCount(0);
std::atomic<long> batchJobCount(0);
std::atomic<long> openJobCount(0);
std::atomic<long> closeJobCount(0);
std::atomic<long> helloWorldCount(0);
std::atomic<long> getVersionCount(0);
std::atomic<long> renewLeaseCount(0);
std::atomic<long> executeCount(0);
std::atomic<long> getExpirationCount(0);
std::atomic<long> getStatusCount(0);
std::atomic<long> getAllJobsCount(0);
std::atomic<long> closeExpiredJobsCount(0);
std::atomic<long> closeAllJobsCount(0);

LOGVARIABLE(RCCServiceInit, 1);
LOGVARIABLE(RCCServiceJobs, 1);
LOGVARIABLE(RCCDataModelInit, 1);

DYNAMIC_FASTFLAGVARIABLE(DebugCrashOnFailToLoadClientSettings, false)
DYNAMIC_FASTFLAGVARIABLE(UseNewSecurityKeyApi, false);
DYNAMIC_FASTFLAGVARIABLE(UseNewMemHashApi, false);
DYNAMIC_FASTSTRINGVARIABLE(MemHashConfig, "");
FASTINTVARIABLE(RCCServiceThreadCount, RBX::TaskScheduler::Threads1)
DYNAMIC_FASTFLAGVARIABLE(US30476, false);
FASTFLAGVARIABLE(UseDataDomain, true);
FASTFLAGVARIABLE(Dep, true)

using json = nlohmann::json;

// Ported from SOAP ns1__LuaValue structure
enum LuaType {
    LUA_TNIL = 0,
    LUA_TBOOLEAN = 1,
    LUA_TNUMBER = 2,
    LUA_TSTRING = 3,
    LUA_TTABLE = 4,
};

static boost::scoped_ptr<MainLogManager> mainLogManager(new MainLogManager("Roblox Web Service", ".dmp", ".crashevent"));

namespace
{
    // isServiceInstalled is Windows-specific, stub for Linux
    static bool isServiceInstalled(const char* svcName)
    {
#ifdef _WIN32
        bool result = false;
        SC_HANDLE scm = OpenSCManager(NULL, NULL, NULL);
        SC_HANDLE thumbService = OpenService(scm, svcName, SERVICE_QUERY_STATUS);
        if (thumbService)
        {
            result = true;
            CloseServiceHandle(thumbService);
        }
        if (scm)
        {
            CloseServiceHandle(scm);
        }
        return result;
#else
        return false; // Always false on Linux
#endif
    }
}

// Job structures (matching SOAP version's internal representation)
struct Job
{
    std::string id;
    double expirationInSeconds;
    int category;
    double cores;

    json to_json() const {
        return json{
            {"id", id},
            {"expirationInSeconds", expirationInSeconds},
            {"category", category},
            {"cores", cores}
        };
    }

    static Job from_json(const json& j) {
        Job job;
        job.id = j.at("id").get<std::string>();
        job.expirationInSeconds = j.at("expirationInSeconds").get<double>();
        job.category = j.at("category").get<int>();
        job.cores = j.at("cores").get<double>();
        return job;
    }
};

struct ScriptExecution
{
    std::string name;
    std::string script;
    // Arguments are now part of the script execution, not the script struct itself
    std::vector<json> arguments; // Use nlohmann::json to hold arbitrary Lua values

    json to_json() const {
        json j = {
            {"name", name},
            {"script", script}
        };
        if (!arguments.empty()) {
            j["arguments"] = arguments;
        }
        return j;
    }

    static ScriptExecution from_json(const json& j) {
        ScriptExecution se;
        se.name = j.value("name", "");
        se.script = j.value("script", "");
        if (j.contains("arguments") && j["arguments"].is_array()) {
            se.arguments = j["arguments"].get<std::vector<json>>();
        }
        return se;
    }
};

// Forward declarations
class CWebService;
class JobItem;

// Job management
class JobItem : boost::noncopyable
{
public:
    enum JobItemRunStatus { RUNNING_JOB, JOB_DONE, JOB_ERROR };

    const std::string id;
    shared_ptr<RBX::DataModel> dataModel;
    RBX::Time expirationTime;
    int category;
    double cores;
    CrossPlatformEvent jobCheckLeaseEvent; // CrossPlatformEvent for Linux compatibility
    rbx::signals::connection notifyAliveConnection;
    rbx::signals::connection closingConnection;
    JobItemRunStatus status;
    std::string errorMessage;

    JobItem(const char* jobId) : id(jobId), jobCheckLeaseEvent(), status(RUNNING_JOB) {} // Initialize event

    void touch(double seconds)
    {
        expirationTime = RBX::Time::now<RBX::Time::Fast>() + RBX::Time::Interval(seconds);
    }

    double secondsToTimeout() const
    {
        return (expirationTime - RBX::Time::now<RBX::Time::Fast>()).seconds();
    }
};

typedef std::map<std::string, shared_ptr<JobItem>> JobMap;

// Security Data Updaters (ported from SOAP version)
class SecurityDataUpdater
{
protected:
    std::string apiUrl;
    std::string data;

    virtual void processDataArray(shared_ptr<const RBX::Reflection::ValueArray> dataArray) = 0;
    bool fetchData()
    {
        std::string newData;
        try
        {
            RBX::Http request(apiUrl);
            request.get(newData);

            // no need to continue if data did not change
            if (newData == data)
                return false;
        }
        catch (std::exception& ex)
        {
            RBX::StandardOut::singleton()->printf(RBX::MESSAGE_WARNING, "SecurityDataUpdater failed to fetch data from %s, %s", apiUrl.c_str(), ex.what());
            return false;
        }

        data = newData;
        return true;
    }

public:
    SecurityDataUpdater(const std::string& url) : apiUrl(url) {}
    virtual ~SecurityDataUpdater() {}

    void run()
    {
        if (!fetchData())
            return;

        shared_ptr<const RBX::Reflection::ValueTable> jsonResult(rbx::make_shared<const RBX::Reflection::ValueTable>());
        if (RBX::WebParser::parseJSONTable(data, jsonResult))
        {
            RBX::Reflection::ValueTable::const_iterator iter = jsonResult->find("data");
            if (iter != jsonResult->end())
            {
                shared_ptr<const RBX::Reflection::ValueArray> dataArray = iter->second.cast<shared_ptr<const RBX::Reflection::ValueArray> >();
                processDataArray(dataArray);
            }
        }
    }
};

class MD5Updater : public SecurityDataUpdater
{
protected:
    void processDataArray(shared_ptr<const RBX::Reflection::ValueArray> dataArray)
    {
        std::set<std::string> hashes;
        for (RBX::Reflection::ValueArray::const_iterator it = dataArray->begin(); it != dataArray->end(); ++it)
        {
            std::string value = it->get<std::string>();
            hashes.insert(value);
        }

        // always add hash for ios
        hashes.insert("ios,ios");

        RBX::Network::Players::setGoldenHashes2(hashes);
    }

public:
    MD5Updater(const std::string& url) : SecurityDataUpdater(url) {}
    ~MD5Updater() {}
};

class SecurityKeyUpdater : public SecurityDataUpdater
{
protected:
    void processDataArray(shared_ptr<const RBX::Reflection::ValueArray> dataArray)
    {
        std::vector<std::string> versions;
        for (RBX::Reflection::ValueArray::const_iterator it = dataArray->begin(); it != dataArray->end(); ++it)
        {
            // version = data + salt
            std::string value = it->get<std::string>();
            std::string version = RBX::sha1(value + "HregBEighE");
            versions.push_back(version);
        }

        RBX::Network::setSecurityVersions(versions);
    }

public:
    SecurityKeyUpdater(const std::string& url) : SecurityDataUpdater(url) {}
    ~SecurityKeyUpdater() {}
};

class MemHashUpdater : public SecurityDataUpdater
{
public:
    static void populateMemHashConfigs(RBX::Network::MemHashConfigs& hashConfigs, const std::string& cfgStr)
    {
        hashConfigs.push_back(RBX::Network::MemHashVector());
        RBX::Network::MemHashVector& thisHashVector = hashConfigs.back();

        std::vector<std::string> hashStrInfo;
        boost::split(hashStrInfo, cfgStr, boost::is_any_of(";"));
        for (std::vector<std::string>::const_iterator hpIt = hashStrInfo.begin(); hpIt != hashStrInfo.end(); ++hpIt)
        {
            std::vector<std::string> args;
            boost::split(args, *hpIt, boost::is_any_of(","));
            if (args.size() >= 3)
            {
                RBX::Network::MemHash thisHash;
                thisHash.checkIdx = boost::lexical_cast<unsigned int>(args[0]);
                thisHash.value = boost::lexical_cast<unsigned int>(args[1]);
                thisHash.failMask = boost::lexical_cast<unsigned int>(args[2]);
                thisHashVector.push_back(thisHash);
            }
        }
    };
protected:
    void processDataArray(shared_ptr<const RBX::Reflection::ValueArray> dataArray)
    {
        RBX::Network::MemHashConfigs hashConfigs;
        for (RBX::Reflection::ValueArray::const_iterator it = dataArray->begin(); it != dataArray->end(); ++it)
        {
            populateMemHashConfigs(hashConfigs, it->get<std::string>());
        }
    }

public:
    MemHashUpdater(const std::string& url) : SecurityDataUpdater(url) {}
    ~MemHashUpdater() {}
};

class RCCServiceSettings : public RBX::FastLogJSON
{
public:
    START_DATA_MAP(RCCServiceSettings);
    DECLARE_DATA_STRING(WindowsMD5);
    DECLARE_DATA_STRING(WindowsPlayerBetaMD5);
    DECLARE_DATA_STRING(MacMD5);
    DECLARE_DATA_INT(SecurityDataTimer);
    DECLARE_DATA_INT(ClientSettingsTimer);
    DECLARE_DATA_STRING(GoogleAnalyticsAccountPropertyID);
    DECLARE_DATA_INT(GoogleAnalyticsThreadPoolMaxScheduleSize);
    DECLARE_DATA_INT(GoogleAnalyticsLoad);
    DECLARE_DATA_BOOL(GoogleAnalyticsInitFix);
    END_DATA_MAP();
};

class RCCServiceDynamicSettings : public RBX::FastLogJSON
{
    typedef std::map<std::string, std::string> FVariables;
    FVariables fVars;

public:
    virtual void ProcessVariable(const std::string& valueName, const std::string& valueData, bool dynamic)
    {
        RBXASSERT(dynamic);
        fVars.insert(std::make_pair(valueName, valueData));
    }

    virtual bool DefaultHandler(const std::string& valueName, const std::string& valueData)
    {
        // only process dynamic flags and logs
        if (valueName[0] == 'D')
            return FastLogJSON::DefaultHandler(valueName, valueData);

        return false;
    }

    void UpdateSettings()
    {
        for (FVariables::iterator i = fVars.begin(); i != fVars.end(); i++)
            FLog::SetValue(i->first, i->second, FASTVARTYPE_DYNAMIC, false);
        fVars.clear();
    }
};


class CWebService : boost::noncopyable
{
public:
    boost::shared_ptr<CProcessPerfCounter> s_perfCounter;
    boost::shared_ptr<RBX::ProfanityFilter> s_profanityFilter;
    boost::scoped_ptr<boost::thread> perfData;
    boost::scoped_ptr<boost::thread> fetchSecurityDataThread;
    boost::scoped_ptr<boost::thread> fetchClientSettingsThread;
    CrossPlatformEvent doneEvent;
    RCCServiceSettings rccSettings;
    RCCServiceDynamicSettings rccDynamicSettings; // Added dynamic settings
    std::string settingsKey;
    std::string securityVersionData;
    std::string clientSettingsData;
    std::string thumbnailSettingsData;
    bool isThumbnailer;

    boost::scoped_ptr<MD5Updater> md5Updater;
    boost::scoped_ptr<SecurityDataUpdater> securityKeyUpdater;
    boost::scoped_ptr<MemHashUpdater> memHashUpdater;
    boost::scoped_ptr<CountersClient> counters;

    JobMap jobs;
    boost::mutex sync;
    boost::mutex currentlyClosingMutex;
    std::atomic<long> dataModelCount;

    crow::SimpleApp app;
    std::unique_ptr<std::thread> serverThread;
    std::atomic<bool> serverStopped;

private:
    void collectPerfData();
    void LoadAppSettings();
    void LoadClientSettings(RCCServiceSettings& dest);
    void LoadClientSettings(std::string& clientDest, std::string& thumbnailDest);
    std::string GetSettingsKey();
    std::vector<std::string> fetchAllowedSecurityVersions();

public:
    static boost::scoped_ptr<CWebService> singleton;

    CWebService(bool crashUploadOnly);
    ~CWebService();

    void startServer(int port);
    void stopServer();
    bool isServerStopped();

    void setupRoutes();
    void validateSecurityData();
    void validateClientSettings();

    // Job management - made public for direct access
    void closeDataModel(shared_ptr<RBX::DataModel> dataModel);
    void notifyAlive(const RBX::Heartbeat& h);
    void setupServerConnections(RBX::DataModel* dataModel);
    shared_ptr<JobItem> createJob(const Job& job, bool startHeartbeat, shared_ptr<RBX::DataModel>& dataModel);
    void doCheckLease(boost::shared_ptr<JobItem> job);
    void contentDataLoaded(shared_ptr<RBX::DataModel>& dataModel);
    JobMap::iterator getJob(const std::string& jobID);

    // Core logic for script execution and job management (ported from SOAP)
    void execute(const std::string& jobID, const ScriptExecution& script, std::vector<json>& result);
    void diag(int type, std::string jobID, std::vector<json>& result);
    void batchJob(const Job& job, const ScriptExecution& script, std::vector<json>& result);
    void asyncExecute(const std::string& id, const ScriptExecution& script, std::vector<json>& result);
    void openJobAndExecuteScript(const Job& job, const ScriptExecution& script, std::vector<json>& result, bool startHeartbeat);

    // API handlers
    crow::response handleHelloWorld();
    crow::response handleGetVersion();
    crow::response handleGetStatus();
    crow::response handleOpenJob(const crow::request& req);
    crow::response handleExecute(const crow::request& req);
    crow::response handleCloseJob(const crow::request& req);
    crow::response handleBatchJob(const crow::request& req);
    crow::response handleRenewLease(const crow::request& req);
    crow::response handleGetExpiration(const crow::request& req);
    crow::response handleGetAllJobs();
    crow::response handleCloseExpiredJobs();
    crow::response handleCloseAllJobs();
    crow::response handleDiag(const crow::request& req);

    // Job management
    void closeJob(const std::string& jobID, const char* errorMessage = nullptr);
    int jobCount();
    void renewLease(const std::string& id, double expirationInSeconds);
    void getExpiration(const std::string& jobID, double* timeout);
    void closeExpiredJobs(int* result);
    void closeAllJobs(int* result);
    void getAllJobs(std::vector<Job>& result);

    // Helper functions for JSON <-> RBX::Reflection::Variant conversion
    json variantToJson(const RBX::Reflection::Variant& source);
    void jsonToVariant(const json& source, RBX::Reflection::Variant& dest);
};

boost::scoped_ptr<CWebService> CWebService::singleton;

void stop_CWebService()
{
    CWebService::singleton.reset();
}

void start_CWebService(const char* contentpath, bool crashUploaderOnly)
{
    // On Linux, contentpath might be relative or absolute.
    // On Windows, PathIsRelative and _AtlBaseModule.m_hInst are used.
    // For cross-platform, we'll assume contentpath is either absolute or relative to executable.
    namespace fs = boost::filesystem;
    fs::path finalContentPath;

#ifdef _WIN32
    TCHAR name[500];
    ::GetModuleFileName(NULL, name, 500); // Using NULL for current executable module handle
    CPath path = name; // CPath is ATL specific, assume a boost::filesystem::path here
    path.RemoveFileSpec();
    finalContentPath = fs::path(path.m_strPath) / contentpath;
#else
    fs::path executablePath;
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        executablePath = fs::path(buffer).parent_path();
    } else {
        executablePath = fs::current_path();
    }

    finalContentPath = fs::path(contentpath);
    if (finalContentPath.is_relative()) {
        finalContentPath = executablePath / finalContentPath;
    }
#endif
    RBX::ContentProvider::setAssetFolder(finalContentPath.string().c_str());

    CWebService::singleton.reset(new CWebService(crashUploaderOnly));
}

RBX_REGISTER_CLASS(ThumbnailGenerator);

void CWebService::collectPerfData()
{
    while (!doneEvent.Wait(1000)) // used as an interruptible sleep.
        s_perfCounter->CollectData();
}

CWebService::CWebService(bool crashUploadOnly) :
    doneEvent(), // Initialize CrossPlatformEvent
    dataModelCount(0)
{
    RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "Intializing Roblox Web Service");

    {
        CVersionInfo vi;
#ifdef _WIN32
        vi.Load(_AtlBaseModule.m_hInst); // Windows-specific
#else
        vi.Load(); // Generic load for Linux
#endif
        RBX::DebugSettings::robloxVersion = vi.GetFileVersionAsDotString();

        RBX::Analytics::setReporter("RCCService");
        RBX::Analytics::setAppVersion(vi.GetFileVersionAsString());
    }

    	s_perfCounter.reset(CProcessPerfCounter::getInstance());
    s_profanityFilter = RBX::ProfanityFilter::getInstance();
    perfData.reset(new boost::thread(RBX::thread_wrapper(boost::bind(&CWebService::collectPerfData, this), "CWebService::collectPerfData")));

    RBX::Http::init(RBX::Http::Curl, RBX::Http::CookieSharingSingleProcessMultipleThreads);

    RBX::Http::requester = "Server";

    RBX::Profiling::init(false);
    static RBX::FactoryRegistrator registerFactoryObjects; // this needs to be here so srand is called before rand

    RBX::Analytics::InfluxDb::init(); // calls rand()

    RobloxCrashReporter::silent = true;
    mainLogManager->WriteCrashDump();

    isThumbnailer = ::isServiceInstalled("Roblox.Thumbnails.Relay");
    LoadAppSettings();

    LoadClientSettings(rccSettings);
    if (rccSettings.GetError())
    {
        RBXCRASH(rccSettings.GetErrorString().c_str());
    }

    RBX::TaskSchedulerSettings::singleton();
    RBX::TaskScheduler::singleton().setThreadCount(RBX::TaskScheduler::ThreadPoolConfig(FInt::RCCServiceThreadCount));

    // Force loading of settings classes
    RBX::GameSettings::singleton();
    RBX::LuaSettings::singleton();
    RBX::DebugSettings::singleton();
    RBX::PhysicsSettings::singleton();

    RBX::Soundscape::SoundService::soundDisabled = true;

    // Initialize the network code
    RBX::Network::initWithServerSecurity();

    //If crashUploadOnly = true, don't create a separate thread of control for uploading
    static DumpErrorUploader dumpErrorUploader(!crashUploadOnly, "RCCService");

    std::string dmpHandlerUrl = GetGridUrl(::GetBaseURL(), FFlag::UseDataDomain);
    dumpErrorUploader.InitCrashEvent(dmpHandlerUrl, mainLogManager->getCrashEventName());
    //dumpErrorUploader.Upload(dmpHandlerUrl);


    fetchClientSettingsThread.reset(new boost::thread(RBX::thread_wrapper(boost::bind(&CWebService::validateClientSettings, this), "CWebService::validateClientSettings")));

    // load and set security versions immediately at start up so it's guaranteed to be there when server is launched
    securityKeyUpdater.reset(new SecurityKeyUpdater(GetSecurityKeyUrl2(GetBaseURL(), "2b4ba7fc-5843-44cf-b107-ba22d3319dcd")));
    md5Updater.reset(new MD5Updater(GetMD5HashUrl(GetBaseURL(), "2b4ba7fc-5843-44cf-b107-ba22d3319dcd")));
    memHashUpdater.reset(new MemHashUpdater(GetMemHashUrl(GetBaseURL(), "2b4ba7fc-5843-44cf-b107-ba22d3319dcd")));

    if (DFFlag::UseNewSecurityKeyApi)
    {
        securityKeyUpdater->run();
    }
    else
    {
        std::vector<std::string> versions = fetchAllowedSecurityVersions();
        RBX::Network::setSecurityVersions(versions);
    }

    md5Updater->run();

    if (DFFlag::UseNewMemHashApi)
    {
        if (DFString::MemHashConfig.size() < 3)
        {
            memHashUpdater->run();
        }
        else
        {

        }
    }

    // this thread uses client setting values, so it must be started AFTER client settings are loaded
    // create a thread to periodically check for security key changes
    fetchSecurityDataThread.reset(new boost::thread(RBX::thread_wrapper(boost::bind(&CWebService::validateSecurityData, this), "CWebService::validateSecurityData")));

    counters.reset(new CountersClient(GetBaseURL().c_str(), "76E5A40C-3AE1-4028-9F10-7C62520BD94F", NULL));

    RBX::ViewBase::InitPluginModules();

#ifdef _WIN32
    if (DFFlag::US30476)
    {
        RBX::initAntiMemDump();
    }
    RBX::initLuaReadOnly();
    RBX::initHwbpVeh();
    if (FFlag::Dep)
    {
        typedef BOOL (WINAPI *SetProcessDEPPolicyPfn)(DWORD);
        SetProcessDEPPolicyPfn pfn= reinterpret_cast<SetProcessDEPPolicyPfn>(GetProcAddress(GetModuleHandle("Kernel32"), "SetProcessDEPPolicy"));
        if (pfn)
        {
            static const DWORD kEnable = 1;
            pfn(kEnable);
        }
    }
#else
    // Initialize Lua on Linux as well
    RBX::initLuaReadOnly();
#endif

    setupRoutes(); // Setup routes after CWebService is fully initialized
}

CWebService::~CWebService()
{
    doneEvent.Set();
    if (perfData) perfData->join();
    if (fetchSecurityDataThread) fetchSecurityDataThread->join();
    if (fetchClientSettingsThread) fetchClientSettingsThread->join();

    RBX::ViewBase::ShutdownPluginModules();
}

std::string CWebService::GetSettingsKey()
{
#ifdef RBX_TEST_BUILD
    // This is a placeholder for test build specific overwrite
    // if (RCCServiceSettingsKeyOverwrite.length() > 0)
    //     return RCCServiceSettingsKeyOverwrite;
#endif

    if (settingsKey.length() == 0)
    {
#ifdef _WIN32
        // Windows-specific registry access
        CRegKey key;
        if (SUCCEEDED(key.Open(HKEY_LOCAL_MACHINE, "Software\\ROBLOX Corporation\\Roblox\\", KEY_READ)))
        {
            CHAR keyData[MAX_PATH];
            ULONG bufLen = MAX_PATH-1;
            if (SUCCEEDED(key.QueryStringValue("SettingsKey", keyData, &bufLen)))
            {
                keyData[bufLen] = 0;
                settingsKey = std::string(keyData);
                FASTLOGS(FLog::RCCServiceInit, "Read settings key: %s", settingsKey);
            }
        }
#else
        // For Linux, we might need an alternative way to get a settings key,
        // or it might be passed via config file/env variable.
        // For now, let's use a placeholder or assume it's empty if not set.
        // Example: settingsKey = "LinuxDefaultSettingsKey";
        FASTLOG(FLog::RCCServiceInit, "No registry settings key on Linux, using default or empty.");
#endif

        if (settingsKey.length() != 0)
            settingsKey = "RCCService" + settingsKey;
        else
            settingsKey = "RCCService";
    }

    return settingsKey;
}

void CWebService::LoadClientSettings(RCCServiceSettings& dest)
{
    // cache settings in a string before processing
    LoadClientSettings(clientSettingsData, thumbnailSettingsData);
    LoadClientSettingsFromString(GetSettingsKey().c_str(), clientSettingsData, &dest);
    if (isThumbnailer)
    {
        LoadClientSettingsFromString("RccThumbnailers", thumbnailSettingsData, &dest);
    }
}

void CWebService::LoadClientSettings(std::string& clientDest, std::string& thumbnailDest)
{
    clientDest.clear();
    thumbnailDest.clear();

    std::string key = GetSettingsKey();
    if (key.length() != 0)
    {
        FetchClientSettingsData(key.c_str(), "D6925E56-BFB9-4908-AAA2-A5B1EC4B2D79", &clientDest);
    }
    if (isThumbnailer)
    {
        FetchClientSettingsData("RccThumbnailers", "D6925E56-BFB9-4908-AAA2-A5B1EC4B2D79", &thumbnailDest);
    }

    bool invalidClientSettings = (clientDest.empty() || key.length() == 0);
    bool invalidThumbnailerSettings = (thumbnailDest.empty() && isThumbnailer);
    if (invalidClientSettings || invalidThumbnailerSettings)
    {
        // hack: we want to log failure to load client settings in GA, but GA init require client setting...
        // so just init GA with default settings, which points to "test" account
        RBX::RobloxGoogleAnalytics::setCanUseAnalytics();
        RBX::RobloxGoogleAnalytics::init(rccSettings.GetValueGoogleAnalyticsAccountPropertyID(), rccSettings.GetValueGoogleAnalyticsThreadPoolMaxScheduleSize());

        char computerName[1024];
        size_t size = 1024;
#ifdef _WIN32
        GetComputerNameA(computerName, (LPDWORD)&size);
#else
        gethostname(computerName, size);
#endif

        if (invalidClientSettings)
        {
            RBX::RobloxGoogleAnalytics::trackEvent(GA_CATEGORY_ERROR, key.length() ? "Empty settings string" : "Empty settings key", computerName);
        }
        else
        {
            RBX::RobloxGoogleAnalytics::trackEvent(GA_CATEGORY_ERROR, "Empty thumbnailer settings string", computerName);
        }

        if (DFFlag::DebugCrashOnFailToLoadClientSettings)
            RBXCRASH();
    }
}

void CWebService::LoadAppSettings()
{
    namespace fs = boost::filesystem;

    fs::path executablePath;
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    executablePath = fs::path(buffer).parent_path();
#else
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        executablePath = fs::path(buffer).parent_path();
    } else {
        executablePath = fs::current_path();
    }
#endif

    FASTLOGS(FLog::RCCServiceInit, "Loading AppSettings.xml, current path: %s", executablePath.string().c_str());

    fs::path appSettingsPath = executablePath / "AppSettings.xml";
    std::ifstream stream(appSettingsPath.string().c_str());

    if (!stream.is_open()) {
        FASTLOGS(FLog::RCCServiceInit, "AppSettings.xml not found at: %s", appSettingsPath.string().c_str());
        // Set default content folder relative to executable
        fs::path defaultContentPath = executablePath / "content";
        RBX::ContentProvider::setAssetFolder(defaultContentPath.string().c_str());
        return;
    }

    try {
        TextXmlParser machine(stream.rdbuf());
        std::unique_ptr<XmlElement> root(machine.parse().release()); // Use unique_ptr

        const XmlElement* baseURLNode = root->findFirstChildByTag(RBX::Name::declare("BaseUrl"));
        if (baseURLNode) {
            std::string baseURL;
            baseURLNode->getValue(baseURL);
            FASTLOGS(FLog::RCCServiceInit, "Got Base url: %s", baseURL.c_str());
            SetBaseURL(baseURL);
        }

        // Parse ContentFolder - added from the HTTP impl snippet, ensuring it's here
        const XmlElement* contentFolderNode = root->findFirstChildByTag(RBX::Name::declare("ContentFolder"));
        if (contentFolderNode) {
            std::string contentFolder;
            contentFolderNode->getValue(contentFolder);
            FASTLOGS(FLog::RCCServiceInit, "Got ContentFolder: %s", contentFolder.c_str());

            fs::path contentPath = fs::path(contentFolder);
            if (contentPath.is_relative()) {
                contentPath = executablePath / contentPath;
            }
            contentPath = contentPath.lexically_normal();

            FASTLOGS(FLog::RCCServiceInit, "Setting asset folder to: %s", contentPath.string().c_str());
            RBX::ContentProvider::setAssetFolder(contentPath.string().c_str());
        } else {
            FASTLOG(FLog::RCCServiceInit, "No ContentFolder found in AppSettings.xml, using default");
            fs::path defaultContentPath = executablePath / "content";
            RBX::ContentProvider::setAssetFolder(defaultContentPath.string().c_str());
        }
    }
    catch (const std::exception& e) {
        FASTLOGS(FLog::RCCServiceInit, "Error parsing AppSettings.xml: %s", e.what());
        // Set default content folder relative to executable
        fs::path defaultContentPath = executablePath / "content";
        RBX::ContentProvider::setAssetFolder(defaultContentPath.string().c_str());
    }
}

std::vector<std::string> CWebService::fetchAllowedSecurityVersions()
{
    const std::string& baseUrl = GetBaseURL();
    if (baseUrl.size() == 0)
        RBXCRASH(); // y u no set BaseURL??

    std::vector<std::string> versions;

    std::string url = GetSecurityKeyUrl(baseUrl, "2b4ba7fc-5843-44cf-b107-ba22d3319dcd");

    RBX::Http request(url);

    std::string allowedSecurityVerionsData = "";
    try
    {
        request.get(allowedSecurityVerionsData);

        // no need to continue if security version did not change
        if (allowedSecurityVerionsData == securityVersionData)
            return std::vector<std::string>();

        // parse json data
        boost::property_tree::ptree pt;
        std::stringstream stream(allowedSecurityVerionsData);
        read_json(stream, pt);

        BOOST_FOREACH(const boost::property_tree::ptree::value_type& child, pt.get_child("data"))
        {
            // version = data + salt
            std::string version = RBX::sha1(child.second.data() + "HregBEighE");
            versions.push_back(version);
        }

        securityVersionData = allowedSecurityVerionsData;

    }
    catch (std::exception ex)
    {
        FASTLOG(FLog::Always, "LoadAllowedPlayerVersions exception");
    }

    return versions;
}

void CWebService::validateClientSettings()
{
	while (!doneEvent.Wait(rccSettings.GetValueClientSettingsTimer() * 1000))
	{
		std::string prevClientSettings = clientSettingsData;
		std::string prevThumbnailSettings = thumbnailSettingsData;

		LoadClientSettings(clientSettingsData, thumbnailSettingsData);

		bool clientChanged = (prevClientSettings != clientSettingsData);
		bool thumbnailChanged = isThumbnailer && (prevThumbnailSettings != thumbnailSettingsData);
		if (clientChanged || thumbnailChanged)
		{
			// collect all dynamic settings
			rccDynamicSettings.ReadFromStream(clientSettingsData.c_str());
			if (isThumbnailer)
			{
				rccDynamicSettings.ReadFromStream(thumbnailSettingsData.c_str());
			}

			// submit datamodel write task to update all dynamic settings
			boost::mutex::scoped_lock lock(sync);
			if (jobs.size() > 0)
			{
				// HACK: Client settings are global, meaning all datamodels use the same set,
				// current we only have 1 datamodel running per rccservice, so submit write task just on first datamodel
				shared_ptr<JobItem> job = jobs.begin()->second;
				job->dataModel->submitTask(boost::bind(&RCCServiceDynamicSettings::UpdateSettings, &rccDynamicSettings), RBX::DataModelJob::Write);
			}
			else
				rccDynamicSettings.UpdateSettings();
		}
	}
}

void CWebService::validateSecurityData()
{
	while (!doneEvent.Wait(rccSettings.GetValueSecurityDataTimer() * 1000))
	{
		if (DFFlag::UseNewSecurityKeyApi)
		{
			if (securityKeyUpdater)
				securityKeyUpdater->run();
		}
		else
		{
			std::string prevVersionData = securityVersionData;
			std::vector<std::string> versions = fetchAllowedSecurityVersions();

			// security versions changed, update all jobs with new versions
			if (prevVersionData != securityVersionData)
				RBX::Network::setSecurityVersions(versions);
		}

		if (DFFlag::UseNewMemHashApi)
		{
			if (memHashUpdater)
			{
				if (DFString::MemHashConfig.size() < 3)
				{
					memHashUpdater->run();
				}
				else
				{

				}
			}
		}

		if (md5Updater)
			md5Updater->run();
	}
}

void CWebService::setupRoutes()
{
    // Hello World endpoint
    CROW_ROUTE(app, "/HelloWorld").methods("POST"_method)
    ([this]() {
        return handleHelloWorld();
    });

    // Get Version endpoint
    CROW_ROUTE(app, "/GetVersion").methods("POST"_method)
    ([this]() {
        return handleGetVersion();
    });

    // Get Status endpoint
    CROW_ROUTE(app, "/GetStatus").methods("POST"_method)
    ([this]() {
        return handleGetStatus();
    });

    // Open Job endpoint
    CROW_ROUTE(app, "/OpenJob").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleOpenJob(req);
    });

    // Execute endpoint
    CROW_ROUTE(app, "/Execute").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleExecute(req);
    });

    // Close Job endpoint
    CROW_ROUTE(app, "/CloseJob").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleCloseJob(req);
    });

    // Batch Job endpoint
    CROW_ROUTE(app, "/BatchJob").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleBatchJob(req);
    });

    // Renew Lease endpoint
    CROW_ROUTE(app, "/RenewLease").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleRenewLease(req);
    });

    // Get Expiration endpoint
    CROW_ROUTE(app, "/GetExpiration").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleGetExpiration(req);
    });

    // Get All Jobs endpoint
    CROW_ROUTE(app, "/GetAllJobs").methods("POST"_method)
    ([this]() {
        return handleGetAllJobs();
    });

    // Close Expired Jobs endpoint
    CROW_ROUTE(app, "/CloseExpiredJobs").methods("POST"_method)
    ([this]() {
        return handleCloseExpiredJobs();
    });

    // Close All Jobs endpoint
    CROW_ROUTE(app, "/CloseAllJobs").methods("POST"_method)
    ([this]() {
        return handleCloseAllJobs();
    });

    // Diag endpoint
    CROW_ROUTE(app, "/Diag").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleDiag(req);
    });
}

void CWebService::startServer(int port)
{
    serverStopped = false;
    app.loglevel(crow::LogLevel::Warning);
    app.port(port);
    serverThread = std::make_unique<std::thread>([this]() {
        app.run();
        serverStopped = true;
    });

    std::cout << "RCCService HTTP API started on port " << port << std::endl;
}

void CWebService::stopServer()
{
    app.stop();
    if (serverThread && serverThread->joinable()) {
        serverThread->join();
    }
}

crow::response CWebService::handleHelloWorld()
{
    helloWorldCount++;
    json response = {
        {"result", "Hello World from RCCService"}
    };
    return crow::response(200, response.dump());
}

crow::response CWebService::handleGetVersion()
{
    getVersionCount++;
    json response = {
        {"result", RBX::DebugSettings::robloxVersion}
    };
    return crow::response(200, response.dump());
}

crow::response CWebService::handleGetStatus()
{
    getStatusCount++;
    json status = {
        {"version", RBX::DebugSettings::robloxVersion},
        {"environmentCount", jobCount()}
    };
    json response = {
        {"result", status}
    };
    return crow::response(200, response.dump());
}

// Helper to convert nlohmann::json to RBX::Reflection::Variant
void CWebService::jsonToVariant(const json& source, RBX::Reflection::Variant& dest) {
    if (source.is_object() && source.contains("type") && source["type"].is_number()) {
        // This is a wrapped LuaValue from the arbiter/backend
        int type = source["type"].get<int>();
        switch (type) {
        case LUA_TNIL:
            dest = RBX::Reflection::Variant();
            break;
        case LUA_TBOOLEAN:
            dest = source.value("value", "false") == "true";
            break;
        case LUA_TNUMBER:
        {
            try {
                dest = std::stod(source.value("value", "0"));
            }
            catch (...) {
                dest = 0.0;
            }
            break;
        }
        case LUA_TSTRING:
            dest = source.value("value", "");
            break;
        case LUA_TTABLE:
            if (source.contains("table") && source["table"].is_array()) {
                shared_ptr<RBX::Reflection::ValueArray> table(new RBX::Reflection::ValueArray(source["table"].size()));
                for (size_t i = 0; i < source["table"].size(); ++i) {
                    jsonToVariant(source["table"][i], (*table)[i]);
                }
                dest = shared_ptr<const RBX::Reflection::ValueArray>(table);
            }
            else {
                dest = shared_ptr<const RBX::Reflection::ValueArray>(new RBX::Reflection::ValueArray());
            }
            break;
        default:
            dest = RBX::Reflection::Variant();
        }
        return;
    }

    if (source.is_null()) {
        dest = RBX::Reflection::Variant(); // Nil
    } else if (source.is_boolean()) {
        dest = source.get<bool>();
    } else if (source.is_number()) {
        dest = source.get<double>(); // Lua numbers are doubles
    } else if (source.is_string()) {
        dest = source.get<std::string>();
    } else if (source.is_array()) {
        shared_ptr<RBX::Reflection::ValueArray> table(new RBX::Reflection::ValueArray(source.size()));
        for (size_t i = 0; i < source.size(); ++i) {
            jsonToVariant(source[i], (*table)[i]);
        }
        dest = shared_ptr<const RBX::Reflection::ValueArray>(table);
    } else if (source.is_object()) {
        // Lua tables can also be maps (objects in JSON)
        shared_ptr<RBX::Reflection::ValueTable> table(new RBX::Reflection::ValueTable());
        for (json::const_iterator it = source.begin(); it != source.end(); ++it) {
            RBX::Reflection::Variant valueVariant;
            jsonToVariant(it.value(), valueVariant);
            table->insert(std::make_pair(it.key(), valueVariant));
        }
        dest = shared_ptr<const RBX::Reflection::ValueTable>(table);
    }
}

// Helper to convert RBX::Reflection::Variant to nlohmann::json
json CWebService::variantToJson(const RBX::Reflection::Variant& source) {
    json j = json::object();
    try {
        if (source.isType<void>()) {
            j["type"] = LUA_TNIL;
        } else if (source.isType<bool>()) {
            j["type"] = LUA_TBOOLEAN;
            j["value"] = source.get<bool>() ? "true" : "false";
        } else if (source.isType<int>()) {
            j["type"] = LUA_TNUMBER;
            j["value"] = std::to_string(source.get<int>());
        } else if (source.isType<float>()) {
            j["type"] = LUA_TNUMBER;
            j["value"] = std::to_string(source.get<float>());
        } else if (source.isType<double>()) {
            j["type"] = LUA_TNUMBER;
            j["value"] = std::to_string(source.get<double>());
        } else if (source.isType<std::string>()) {
            j["type"] = LUA_TSTRING;
            j["value"] = source.get<std::string>();
        } else if (source.isType<RBX::ContentId>()) {
            j["type"] = LUA_TSTRING;
            j["value"] = source.get<std::string>();
        } else if (source.isType<shared_ptr<const RBX::Reflection::ValueArray>>()) {
            j["type"] = LUA_TTABLE;
            json arr = json::array();
            shared_ptr<const RBX::Reflection::ValueArray> collection = source.cast<shared_ptr<const RBX::Reflection::ValueArray>>();
            for (size_t i = 0; i < collection->size(); ++i) {
                arr.push_back(variantToJson((*collection)[i]));
            }
            j["table"] = arr;
        } else if (source.isType<shared_ptr<const RBX::Reflection::ValueTable>>()) {
            j["type"] = LUA_TTABLE;
            json arr = json::array();
            shared_ptr<const RBX::Reflection::ValueTable> table = source.cast<shared_ptr<const RBX::Reflection::ValueTable>>();
            for (RBX::Reflection::ValueTable::const_iterator it = table->begin(); it != table->end(); ++it) {
                arr.push_back(variantToJson(it->second));
            }
            j["table"] = arr;
        } else {
            j["type"] = LUA_TNIL;
            RBX::StandardOut::singleton()->printf(RBX::MESSAGE_WARNING, "variantToJson: Unknown type in variant");
        }
    } catch (const std::exception& e) {
        j["type"] = LUA_TSTRING;
        j["value"] = std::string("Error converting variant: ") + e.what();
        RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "variantToJson EXCEPTION: %s", e.what());
    }
    return j;
}

// Ported execute logic from SOAP CWebService
void CWebService::execute(const std::string& jobID, const ScriptExecution& script, std::vector<json>& result)
{
    std::string code = script.script;
    if (RBX::ContentProvider::isHttpUrl(code))
    {
        RBX::Http http(code);
        http.get(code); // Blocking call to fetch script content
    }

    shared_ptr<RBX::DataModel> dataModel;
    {
        boost::mutex::scoped_lock lock(sync);
        JobMap::iterator iter = getJob(jobID);
        dataModel = iter->second->dataModel;
    }

    	std::unique_ptr<const RBX::Reflection::Tuple> tuple;
    {
        RBX::Reflection::Tuple args(script.arguments.size());
        for (size_t i = 0; i < script.arguments.size(); ++i)
            jsonToVariant(script.arguments[i], args.values[i]);

        RBX::DataModel::LegacyLock lock(dataModel, RBX::DataModelJob::Write);
        if (dataModel->isClosed())
            throw std::runtime_error("The DataModel is closed");
        RBX::ScriptContext* scriptContext = RBX::ServiceProvider::create<RBX::ScriptContext>(dataModel.get());
        tuple = scriptContext->executeInNewThread(RBX::Security::WebService, RBX::ProtectedString::fromTrustedSource(code), script.name.c_str(), args);
    }

    result.clear();
    for (size_t i = 0; i < tuple->values.size(); ++i)
    {
        result.push_back(variantToJson(tuple->values[i]));
    }
}

// Ported diag logic from SOAP CWebService
void CWebService::diag(int type, std::string jobID, std::vector<json>& result)
{
    	std::unique_ptr<const RBX::Reflection::ValueArray> tuple_variant;

    {
        shared_ptr<RBX::DataModel> dataModel;
        if (!jobID.empty())
        {
            boost::mutex::scoped_lock lock(sync);
            JobMap::iterator iter = getJob(jobID);
            dataModel = iter->second->dataModel;
        }
        // This is the internal diag function from SOAP CWebService
        // Gathers all non-global Instances that are in the heap
        auto arbiterActivityDump = [&](RBX::Reflection::ValueArray& arr_result) {
            boost::mutex::scoped_lock lock(sync);
            for (JobMap::iterator iter = jobs.begin(); iter != jobs.end(); ++iter)
            {
                shared_ptr<RBX::DataModel> dataModel = iter->second->dataModel;
                shared_ptr<RBX::Reflection::ValueArray> tuple(new RBX::Reflection::ValueArray());
                tuple->push_back(dataModel->arbiterName());
                tuple->push_back(dataModel->getAverageActivity());
                arr_result.push_back(shared_ptr<const RBX::Reflection::ValueArray>(tuple));
            }
        };

        auto leakDump = [&](RBX::Reflection::ValueArray& arr_result) {
            // This was empty in the SOAP version, keeping it empty for now
        };

        		std::unique_ptr<RBX::Reflection::ValueArray> tuple(new RBX::Reflection::ValueArray());

        /* This is the format of the Diag data:

            type == 0
                DataModel Count in this process
                PerfCounter data
                Task Scheduler
                    (obsolete entry)
                    double threadAffinity
                    double numQueuedJobs
                    double numScheduledJobs
                    double numRunningJobs
                    long threadPoolSize
                    double messageRate
                    double messagePumpDutyCycle
                DataModel Jobs Info
                Machine configuration
                Memory Leak Detection
            type & 1
                leak dump
            type & 2
                attempt to allocate 500k. if success, then true else false
            type & 4
                DataModel dutyCycles
        */
        tuple->push_back((double)dataModelCount.load()); // Use .load() for atomic

        {
            shared_ptr<RBX::Reflection::ValueArray> perfCounterData(new RBX::Reflection::ValueArray());
            perfCounterData->push_back((double)s_perfCounter->GetProcessCores());
            perfCounterData->push_back((double)s_perfCounter->GetTotalProcessorTime());
            perfCounterData->push_back((double)s_perfCounter->GetProcessorTime());
            perfCounterData->push_back((double)s_perfCounter->GetPrivateBytes());
            perfCounterData->push_back((double)s_perfCounter->GetPrivateWorkingSetBytes());
            perfCounterData->push_back(-1.0);
            perfCounterData->push_back(-1.0);
            perfCounterData->push_back((double)s_perfCounter->GetElapsedTime());
            perfCounterData->push_back((double)s_perfCounter->GetVirtualBytes());
            perfCounterData->push_back((double)s_perfCounter->GetPageFileBytes());
            perfCounterData->push_back((double)s_perfCounter->GetPageFaultsPerSecond());

            tuple->push_back(shared_ptr<const RBX::Reflection::ValueArray>(perfCounterData));
        }
        {
            shared_ptr<RBX::Reflection::ValueArray> taskSchedulerData(new RBX::Reflection::ValueArray());
            taskSchedulerData->push_back(0.0);    // obsolete
            taskSchedulerData->push_back((double)RBX::TaskScheduler::singleton().threadAffinity());
            taskSchedulerData->push_back((double)RBX::TaskScheduler::singleton().numSleepingJobs());
            taskSchedulerData->push_back((double)RBX::TaskScheduler::singleton().numWaitingJobs());
            taskSchedulerData->push_back((double)RBX::TaskScheduler::singleton().numRunningJobs());
            taskSchedulerData->push_back((double)RBX::TaskScheduler::singleton().threadPoolSize());
            taskSchedulerData->push_back((double)RBX::TaskScheduler::singleton().schedulerRate());
            taskSchedulerData->push_back((double)RBX::TaskScheduler::singleton().getSchedulerDutyCyclePerThread());
            tuple->push_back(shared_ptr<const RBX::Reflection::ValueArray>(taskSchedulerData));
        }

        if (dataModel)
            tuple->push_back(dataModel->getJobsInfo());
        else
            tuple->push_back(0.0); // Or an empty array/table if that's more appropriate for "no data model"

        {
            shared_ptr<RBX::Reflection::ValueArray> machineData(new RBX::Reflection::ValueArray());
            machineData->push_back((double)RBX::DebugSettings::singleton().totalPhysicalMemory());
            machineData->push_back(RBX::DebugSettings::singleton().cpu());
            machineData->push_back((double)RBX::DebugSettings::singleton().cpuCount());
            machineData->push_back((double)RBX::DebugSettings::singleton().cpuSpeed());
            tuple->push_back(shared_ptr<const RBX::Reflection::ValueArray>(machineData));
        }

        {
            // TODO, use RBX::poolAllocationList and RBX::poolAvailablityList to get complete stats
            shared_ptr<RBX::Reflection::ValueArray> memCounters(new RBX::Reflection::ValueArray());
            memCounters->push_back((double)RBX::Diagnostics::Countable<RBX::Instance>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<RBX::PartInstance>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<RBX::ModelInstance>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<RBX::Explosion>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<RBX::Soundscape::SoundChannel>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<rbx::signals::connection>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<rbx::signals::connection::islot>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<RBX::Reflection::GenericSlotWrapper>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<RBX::TaskScheduler::Job>::getCount());
            memCounters->push_back((double)RBX::Diagnostics::Countable<RBX::Network::Player>::getCount());
#ifdef RBX_ALLOCATOR_COUNTS
            memCounters->push_back((double)RBX::Allocator<RBX::Body>::getCount());
            memCounters->push_back((double)RBX::Allocator<RBX::Cofm>::getCount());
            memCounters->push_back((double)RBX::Allocator<RBX::NormalBreakConnector>::getCount());
            memCounters->push_back((double)RBX::Allocator<RBX::ContactConnector>::getCount());
            memCounters->push_back((double)RBX::Allocator<RBX::RevoluteLink>::getCount());
            memCounters->push_back((double)RBX::Allocator<RBX::SimBody>::getCount());
            memCounters->push_back((double)RBX::Allocator<RBX::BallBallConnector>::getCount());
            memCounters->push_back((double)RBX::Allocator<RBX::BallBlockConnector>::getCount());
            memCounters->push_back((double)RBX::Allocator<RBX::BlockBlockContact>::getCount());
            memCounters->push_back((double)RBX::Allocator<XmlAttribute>::getCount());
            memCounters->push_back((double)RBX::Allocator<XmlElement>::getCount());
#else
            for(int i = 0; i<11; ++i)
                memCounters->push_back(-1.0);
#endif
            memCounters->push_back((double)ThumbnailGenerator::totalCount);
            tuple->push_back(shared_ptr<const RBX::Reflection::ValueArray>(memCounters));
        }

        if (type & 1)
        {
            shared_ptr<RBX::Reflection::ValueArray> leak_result_arr(new RBX::Reflection::ValueArray());
            leakDump(*leak_result_arr);
            tuple->push_back(shared_ptr<const RBX::Reflection::ValueArray>(leak_result_arr));
        }

        if (type & 2)
        {
            try
            {
                {
                    std::vector<char> v1(100000);
                    std::vector<char> v2(100000);
                    std::vector<char> v3(100000);
                    std::vector<char> v4(100000);
                    std::vector<char> v5(100000);
                }
                tuple->push_back(true);
            }
            catch (...)
            {
                tuple->push_back(false);
            }
        }

        if (type & 4)
        {
            shared_ptr<RBX::Reflection::ValueArray> arbiter_result_arr(new RBX::Reflection::ValueArray());
            arbiterActivityDump(*arbiter_result_arr);
            tuple->push_back(shared_ptr<const RBX::Reflection::ValueArray>(arbiter_result_arr));
        }

        tuple_variant.reset(tuple.release());
    }

    result.clear();
    for (size_t i = 0; i < tuple_variant->size(); ++i)
    {
        result.push_back(variantToJson((*tuple_variant)[i]));
    }
}

crow::response CWebService::handleOpenJob(const crow::request& req)
{
    openJobCount++;
    try {
        json requestJson = json::parse(req.body);
        Job job = Job::from_json(requestJson["job"]);
        ScriptExecution script = ScriptExecution::from_json(requestJson["script"]); // Script is now part of openJob request

        std::vector<json> result_json;
        openJobAndExecuteScript(job, script, result_json, true); // Pass true for startHeartbeat as in SOAP OpenJob

        json response = {
            {"success", true},
            {"jobId", job.id},
            {"result", result_json} // Return script execution result
        };
        return crow::response(200, response.dump());
    }
    catch (const std::exception& e) {
        json error = {
            {"error", e.what()}
        };
        return crow::response(400, error.dump());
    }
}

crow::response CWebService::handleExecute(const crow::request& req)
{
    executeCount++;
    try {
        json requestJson = json::parse(req.body);
        std::string jobId = requestJson.contains("jobId") ? requestJson["jobId"].get<std::string>() : requestJson.value("jobID", "");
        ScriptExecution script = ScriptExecution::from_json(requestJson["script"]);

        // Check if job exists
        {
            boost::mutex::scoped_lock lock(sync);
            auto it = jobs.find(jobId);
            if (it == jobs.end()) {
                json error = {
                    {"error", "Job not found"}
                };
                return crow::response(404, error.dump());
            }
        }

        // Use std::async to run execute asynchronously with a timeout
        auto future = std::async(std::launch::async, [this, jobId, script]() -> std::string {
            try {
                std::vector<json> execution_result;
                execute(jobId, script, execution_result);
                json response = {
                    {"success", true},
                    {"result", execution_result}
                };
                return response.dump();
            } catch (const std::exception& e) {
                json error = {
                    {"error", e.what()}
                };
                return error.dump();
            }
        });

        // Wait for the result with a timeout (e.g., 30 seconds)
        std::chrono::seconds timeout(30);
        if (future.wait_for(timeout) == std::future_status::ready) {
            std::string result = future.get();
            // Check if it's an error response
            json resultJson = json::parse(result);
            if (resultJson.contains("error")) {
                return crow::response(400, result);
            } else {
                return crow::response(200, result);
            }
        } else {
            // Timeout occurred
            json error = {
                {"error", "Script execution timed out"}
            };
            return crow::response(408, error.dump()); // 408 Request Timeout
        }
    }
    catch (const std::exception& e) {
        json error = {
            {"error", e.what()}
        };
        return crow::response(400, error.dump());
    }
}

crow::response CWebService::handleCloseJob(const crow::request& req)
{
    closeJobCount++;
    try {
        json requestJson = json::parse(req.body);
        std::string jobId = requestJson.contains("jobId") ? requestJson["jobId"].get<std::string>() : requestJson.value("jobID", "");

        closeJob(jobId);

        json response = {
            {"success", true}
        };
        return crow::response(200, response.dump());
    }
    catch (const std::exception& e) {
        json error = {
            {"error", e.what()}
        };
        return crow::response(400, error.dump());
    }
}

// Ported batchJob logic from SOAP CWebService
void CWebService::batchJob(const Job& job, const ScriptExecution& script, std::vector<json>& result)
{
    std::string id = job.id;

    shared_ptr<RBX::DataModel> dataModel;
    dataModelCount.fetch_add(1); // Use atomic increment
    try
    {
        shared_ptr<JobItem> j = createJob(job, false, dataModel); // false for startHeartbeat, as it's batch

        FASTLOGS(FLog::RCCServiceJobs, "Opened Batch JobItem %s", id.c_str());
        FASTLOG1(FLog::RCCServiceJobs, "DataModel: %p", dataModel.get());

        // Spin off a thread for the BatchJob to execute in
        // Use a detached thread as in the SOAP version's asyncExecute
        std::thread([this, id, script, &result]() {
            asyncExecute(id, script, result);
        }).detach();

        while (true)
        {
            double sleepTimeInSeconds = j->secondsToTimeout();

            if (sleepTimeInSeconds <= 0) // This case seems like an edge case (we already ran out of time)
            {
                if (j->jobCheckLeaseEvent.Wait(1)) { // Check if event is set
                    if (j->status == JobItem::JOB_ERROR)
                        throw RBX::runtime_error(j->errorMessage.c_str());
                }

                closeJob(id);
                throw RBX::runtime_error("BatchJob Timeout");
            }
            else if (!j->jobCheckLeaseEvent.Wait((int)(sleepTimeInSeconds * 1000.0) + 3)) // Wait for event or timeout
            {
                // do nothing, just continue looping. Probably sleepTimeInSeconds will be negative, and we'll throw
            }
            else
            {
                // jobCheckLeaseEvent was set
                if (j->status == JobItem::JOB_ERROR)
                    throw RBX::runtime_error(j->errorMessage.c_str());

                closeJob(id);
                return;
            }
        }
    }
    catch (std::exception&)
    {
        throw;
    }
}

// Ported asyncExecute logic from SOAP CWebService
void CWebService::asyncExecute(const std::string& id, const ScriptExecution& script, std::vector<json>& result)
{
    try
    {
        this->execute(id, script, result);
        closeJob(id);
    }
    catch (std::exception& e)
    {
        // Use std::string for error message
        std::string szMsg = e.what();
        closeJob(id, szMsg.c_str());
    }
}


crow::response CWebService::handleBatchJob(const crow::request& req)
{
    batchJobCount++;
    try {
        json requestJson = json::parse(req.body);
        Job job = Job::from_json(requestJson["job"]);
        ScriptExecution script = ScriptExecution::from_json(requestJson["script"]);

        std::vector<json> result_json;
        batchJob(job, script, result_json); // Call the ported batchJob logic

        json response = {
            {"success", true},
            {"result", result_json} // Return batch job result
        };
        return crow::response(200, response.dump());
    }
    catch (const std::exception& e) {
        json error = {
            {"error", e.what()}
        };
        return crow::response(400, error.dump());
    }
}

crow::response CWebService::handleRenewLease(const crow::request& req)
{
    renewLeaseCount++;
    try {
        json requestJson = json::parse(req.body);
        std::string jobId = requestJson.contains("jobId") ? requestJson["jobId"].get<std::string>() : requestJson.value("jobID", "");
        double expirationInSeconds = requestJson["expirationInSeconds"];

        renewLease(jobId, expirationInSeconds);

        json response = {
            {"success", true}
        };
        return crow::response(200, response.dump());
    }
    catch (const std::exception& e) {
        json error = {
            {"error", e.what()}
        };
        return crow::response(400, error.dump());
    }
}

crow::response CWebService::handleGetExpiration(const crow::request& req)
{
    getExpirationCount++;
    try {
        json requestJson = json::parse(req.body);
        std::string jobId = requestJson.contains("jobId") ? requestJson["jobId"].get<std::string>() : requestJson.value("jobID", "");

        double timeout;
        getExpiration(jobId, &timeout);

        json response = {
            {"result", timeout}
        };
        return crow::response(200, response.dump());
    }
    catch (const std::exception& e) {
        json error = {
            {"error", e.what()}
        };
        return crow::response(400, error.dump());
    }
}

crow::response CWebService::handleGetAllJobs()
{
    getAllJobsCount++;
    std::vector<Job> jobsVec;
    getAllJobs(jobsVec);

    json jobsArray = json::array();
    for (const auto& job : jobsVec) {
        jobsArray.push_back(job.to_json());
    }

    json response = {
        {"result", jobsArray}
    };
    return crow::response(200, response.dump());
}

crow::response CWebService::handleCloseExpiredJobs()
{
    closeExpiredJobsCount++;
    int closedCount = 0;
    closeExpiredJobs(&closedCount);

    json response = {
        {"result", closedCount}
    };
    return crow::response(200, response.dump());
}

crow::response CWebService::handleCloseAllJobs()
{
    closeAllJobsCount++;
    int closedCount = 0;
    closeAllJobs(&closedCount);

    json response = {
        {"result", closedCount}
    };
    return crow::response(200, response.dump());
}

crow::response CWebService::handleDiag(const crow::request& req)
{
    diagCount++;
    try {
        json requestJson = json::parse(req.body);
        int type = requestJson.value("type", 0);
        std::string jobId = requestJson.contains("jobId") ? requestJson["jobId"].get<std::string>() : requestJson.value("jobID", "");

        std::vector<json> diag_result;
        diag(type, jobId, diag_result);

        json response = {
            {"success", true},
            {"result", diag_result}
        };
        return crow::response(200, response.dump());
    }
    catch (const std::exception& e) {
        RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "handleDiag EXCEPTION: %s", e.what());
        json error = {
            {"error", e.what()}
        };
        return crow::response(400, error.dump());
    }
}

void CWebService::closeJob(const std::string& jobID, const char* errorMessage)
{
    // Take a mutex for the duration of closeJob here. The arbiter thinks that as
    // soon as closeJob returns it is safe to force kill the rcc process, so make
    // sure that if it is in the process of closing the datamodel any parallel requests
    // to close the data model block at the mutex until the first one finishes.
    boost::mutex::scoped_lock closeLock(currentlyClosingMutex);

    shared_ptr<RBX::DataModel> dataModel;
    {
        boost::mutex::scoped_lock lock(sync);
        JobMap::iterator iter = jobs.find(jobID);
        if (iter != jobs.end())
        {
            if (errorMessage) {
                iter->second->errorMessage = errorMessage;
                iter->second->status = JobItem::JOB_ERROR;
            }
            else {
                iter->second->status = JobItem::JOB_DONE;
            }
            iter->second->jobCheckLeaseEvent.Set();
            dataModel = iter->second->dataModel;
            iter->second->notifyAliveConnection.disconnect();
            iter->second->closingConnection.disconnect();
            jobs.erase(iter);
        }
    }

    if (dataModel)
    {
        FASTLOGS(FLog::RCCServiceJobs, "Closing JobItem %s", jobID.c_str());
        FASTLOG1(FLog::RCCServiceJobs, "DataModel: %p", dataModel.get());

        closeDataModel(dataModel);
        dataModel.reset();
        if (dataModelCount.fetch_sub(1) == 1) { // Use atomic decrement
            if (mainLogManager) {
                mainLogManager->DisableHangReporting();
            }
        }
    }
}

void CWebService::closeDataModel(shared_ptr<RBX::DataModel> dataModel)
{
    RBX::Security::Impersonator impersonate(RBX::Security::WebService);
    RBX::DataModel::closeDataModel(dataModel);
}

void CWebService::notifyAlive(const RBX::Heartbeat& h)
{
    mainLogManager->NotifyFGThreadAlive();
}

int CWebService::jobCount()
{
    boost::mutex::scoped_lock lock(sync);
    return jobs.size();
}

void CWebService::setupServerConnections(RBX::DataModel* dataModel)
{
    if (!dataModel) {
        return;
    }

    if (RBX::AdService* adService = dataModel->create<RBX::AdService>()) {
        adService->sendServerVideoAdVerification.connect(boost::bind(&RBX::AdService::checkCanPlayVideoAd, adService, _1, _2));
        adService->sendServerRecordImpression.connect(boost::bind(&RBX::AdService::sendAdImpression, adService, _1, _2, _3));
    }
}

shared_ptr<JobItem> CWebService::createJob(const Job& job, bool startHeartbeat, shared_ptr<RBX::DataModel>& dataModel)
{
    srand(RBX::randomSeed()); // make sure this thread is seeded
    std::string id = job.id;

    dataModel = RBX::DataModel::createDataModel(startHeartbeat, new RBX::NullVerb(NULL,""), false);
    setupServerConnections(dataModel.get());

    RBX::Network::Players* players = dataModel->find<RBX::Network::Players>();
    if (players) {
        LoadClientSettings(rccSettings);
        if (rccSettings.GetError()) {
            RBXCRASH(rccSettings.GetErrorString().c_str());
        }

        RBX::Http::SetUseStatistics(true);

        FASTLOGS(FLog::RCCDataModelInit, "Creating Data Model, Windows MD5: %s", rccSettings.GetValueWindowsMD5());
        FASTLOGS(FLog::RCCDataModelInit, "Creating Data Model, Mac MD5: %s", rccSettings.GetValueMacMD5());
        FASTLOGS(FLog::RCCDataModelInit, "Creating Data Model, Windows Player Beta MD5: %s", rccSettings.GetValueWindowsPlayerBetaMD5());

        if (rccSettings.GetValueGoogleAnalyticsInitFix()) {
            RBX::Analytics::GoogleAnalytics::lotteryInit(rccSettings.GetValueGoogleAnalyticsAccountPropertyID(), rccSettings.GetValueGoogleAnalyticsLoad());
        } else {
            int lottery = rand() % 100;
            FASTLOG1(FLog::RCCDataModelInit, "Google analytics lottery number = %d", lottery);
            if (lottery < rccSettings.GetValueGoogleAnalyticsLoad()) {
                FASTLOG1(FLog::RCCDataModelInit, "Setting Google Analytics ThreadPool Max Schedule Size: %d", rccSettings.GetValueGoogleAnalyticsThreadPoolMaxScheduleSize());
                FASTLOGS(FLog::RCCDataModelInit, "Setting Google Analytics Account Property ID: %s", rccSettings.GetValueGoogleAnalyticsAccountPropertyID());
                RBX::RobloxGoogleAnalytics::setCanUseAnalytics();
                RBX::RobloxGoogleAnalytics::init(rccSettings.GetValueGoogleAnalyticsAccountPropertyID(), rccSettings.GetValueGoogleAnalyticsThreadPoolMaxScheduleSize());
            }
        }

        RBX::DataModel::LegacyLock lock(dataModel, RBX::DataModelJob::Write);
        dataModel->create<RBX::Network::WebChatFilter>();
    }

    dataModel->workspaceLoadedSignal.connect(boost::bind(&CWebService::contentDataLoaded, this, dataModel));

    dataModel->jobId = id;
    shared_ptr<JobItem> j(new JobItem(id.c_str()));
    j->dataModel = dataModel;
    j->closingConnection = dataModel->closingSignal.connect(boost::bind(&CWebService::closeJob, this, id, "DataModel closed"));
    j->category = job.category;
    j->cores = job.cores;
    j->touch(job.expirationInSeconds);

    {
        boost::mutex::scoped_lock lock(sync);
        if (jobs.find(id) != jobs.end())
            throw RBX::runtime_error("JobItem %s already exists", id.c_str());
        jobs[id] = j;
    }

    return j;
}

void CWebService::doCheckLease(boost::shared_ptr<JobItem> job)
{
    std::string id = job->id;
    try {
        while (true) {
            double sleepTimeInSeconds = job->secondsToTimeout();
            if (sleepTimeInSeconds <= 0) {

                closeJob(id, "Lease expired");
                return;
            } else if (job->jobCheckLeaseEvent.Wait((int)(sleepTimeInSeconds * 1000.0) + 3)) { // Use CrossPlatformEvent::Wait
                return;
            }
        }
    } catch (std::exception& e) {
        FASTLOGS(FLog::RCCServiceJobs, "doCheckLease failure: %s", e.what());
    }
}

void CWebService::contentDataLoaded(shared_ptr<RBX::DataModel>& dataModel)
{
    RBX::CSGDictionaryService* dictionaryService = RBX::ServiceProvider::create<RBX::CSGDictionaryService>(dataModel.get());
    RBX::NonReplicatedCSGDictionaryService* nrDictionaryService = RBX::ServiceProvider::create<RBX::NonReplicatedCSGDictionaryService>(dataModel.get());
    dictionaryService->reparentAllChildData();
}

JobMap::iterator CWebService::getJob(const std::string& jobID)
{
    JobMap::iterator iter = jobs.find(jobID);
    if (iter == jobs.end()) {
        RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "getJob: JobItem '%s' not found. Available jobs: %zu", jobID.c_str(), jobs.size());
        for (auto const& [id, item] : jobs) {
            RBX::StandardOut::singleton()->printf(RBX::MESSAGE_INFO, "  available job: '%s'", id.c_str());
        }
        throw RBX::runtime_error("JobItem %s not found", jobID.c_str());
    }
    return iter;
}

// Ported openJob logic from SOAP CWebService
void CWebService::openJobAndExecuteScript(const Job& job, const ScriptExecution& script, std::vector<json>& result, bool startHeartbeat)
{
    std::string id = job.id;
    RBX::Http::gameID = job.id;

    shared_ptr<RBX::DataModel> dataModel;
    dataModelCount.fetch_add(1); // Use atomic increment
    try
    {
        shared_ptr<JobItem> j = createJob(job, startHeartbeat, dataModel);

        try
        {
            RBX::DataModel *pDataModel = dataModel.get();

            // Monitor the job and close it if needed
            boost::thread(RBX::thread_wrapper(boost::bind(&CWebService::doCheckLease, this, j), "Check Expiration")).detach();

            FASTLOGS(FLog::RCCServiceJobs, "Opened JobItem %s", id.c_str());
            FASTLOG1(FLog::RCCServiceJobs, "DataModel: %p", pDataModel);

            this->execute(id, script, result); // Execute the script

            RBX::RunService* runService = RBX::ServiceProvider::find<RBX::RunService>(pDataModel);
            RBXASSERT(runService != NULL);
            j->notifyAliveConnection = runService->heartbeatSignal.connect(boost::bind(&CWebService::notifyAlive, this, _1));

            {
                RBX::DataModel::LegacyLock lock(pDataModel, RBX::DataModelJob::Write);
                pDataModel->loadCoreScripts();
            }
        }
        catch (std::exception& e)
        {
            std::string szMsg = e.what();
            boost::mutex::scoped_lock lock(sync);
            jobs.erase(id);
            throw;
        }
    }
    catch (std::exception& e)
    {
        std::string szMsg = e.what();

        FASTLOGS(FLog::RCCServiceJobs, "Closing DataModel due to exception: %s", szMsg.c_str());
        FASTLOG1(FLog::RCCServiceJobs, "DataModel: %p", dataModel.get());

        closeDataModel(dataModel);
        dataModel.reset();
        dataModelCount.fetch_sub(1); // Use atomic decrement
        throw;
    }
}

void CWebService::renewLease(const std::string& id, double expirationInSeconds)
{
    boost::mutex::scoped_lock lock(sync);
    getJob(id)->second->touch(expirationInSeconds);
}

void CWebService::getExpiration(const std::string& jobID, double * timeout)
{
    boost::mutex::scoped_lock lock(sync);
    *timeout = getJob(jobID)->second->secondsToTimeout();
}

void CWebService::closeExpiredJobs(int* result)
{
    std::vector<std::string> jobsToClose;
    {
        boost::mutex::scoped_lock lock(sync);
        JobMap::iterator end = jobs.end();
        for (JobMap::iterator iter = jobs.begin(); iter != end; ++iter)
            if (iter->second->secondsToTimeout() <= 0)
                jobsToClose.push_back(iter->first);
    }
    *result = jobsToClose.size();
    std::for_each(jobsToClose.begin(), jobsToClose.end(), boost::bind(&CWebService::closeJob, this, _1, (const char*)NULL));
}

void CWebService::closeAllJobs(int* result)
{
    FASTLOG(FLog::RCCServiceJobs, "Closing all jobs command");

    std::vector<std::string> jobsToClose;
    {
        boost::mutex::scoped_lock lock(sync);
        JobMap::iterator end = jobs.end();
        for (JobMap::iterator iter = jobs.begin(); iter != end; ++iter)
            jobsToClose.push_back(iter->first);
    }
    *result = jobsToClose.size();
    std::for_each(jobsToClose.begin(), jobsToClose.end(), boost::bind(&CWebService::closeJob, this, _1, (const char*)NULL));
}

void CWebService::getAllJobs(std::vector<Job>& result)
{
    boost::mutex::scoped_lock lock(sync);
    result.clear();

    JobMap::iterator iter = jobs.begin();
    JobMap::iterator end = jobs.end();
    while (iter != end)
    {
        Job job;
        job.expirationInSeconds = iter->second->secondsToTimeout();
        job.category = iter->second->category;
        job.cores = iter->second->cores;
        job.id = iter->first;
        result.push_back(job);
        ++iter;
    }
}

bool CWebService::isServerStopped() { return serverStopped.load(); }

DATA_MAP_IMPL_START(RCCServiceSettings)
IMPL_DATA(WindowsMD5, "");
IMPL_DATA(MacMD5, "");
IMPL_DATA(SecurityDataTimer, 300);
IMPL_DATA(ClientSettingsTimer, 300);
IMPL_DATA(GoogleAnalyticsAccountPropertyID, "");
IMPL_DATA(GoogleAnalyticsThreadPoolMaxScheduleSize, 100);
IMPL_DATA(GoogleAnalyticsLoad, 100);
IMPL_DATA(GoogleAnalyticsInitFix, false);
DATA_MAP_IMPL_END()
