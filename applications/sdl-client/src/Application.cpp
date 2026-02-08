#include "SDL3/SDL.h"
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include "util/RegistryUtil.h"

static HWND GetHwnd(SDL_Window* window) {
    return (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}
#endif
#include <boost/filesystem/operations.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

#include "Application.h"
#include "AuthenticationMarshallar.h"
#include "Crypt.h"
#include "Document.h"
#include "DumpErrorUploader.h"
#include "RobloxServicesTools.h"
#include "FunctionMarshaller.h"
#include "gui/ProfanityFilter.h"
#include "util/FileSystem.h"
#include "util/Http.h"
#include "util/MachineIdUploader.h"
#include "util/MD5Hasher.h"
#include "util/SoundService.h"
#include "util/Statistics.h"
#include "util/RobloxGoogleAnalytics.h"
#include "util/SafeToLower.h"
#include "rbx/ProcessPerfCounter.h"
#include "rbx/Tasks/Coordinator.h"
// #include "resource.h" // Windows resource, remove or replace
#include "RenderSettingsItem.h"
#include "script/CoreScript.h"
#include "v8datamodel/ContentProvider.h"
#include "v8datamodel/DebugSettings.h"
#include "v8datamodel/Game.h"
#include "v8datamodel/HackDefines.h"
#include "v8xml/XmlSerializer.h"
#include "format_string.h"
// #include "VistaTools.h" // Windows-specific, remove
#include "MachineConfiguration.h"
#include "reflection/reflection.h"
#include "ReflectionMetadata.h"
#include "v8datamodel/FastLogSettings.h"
#include "network/api.h"
#include "StringConv.h"
#include "v8datamodel/DataModel.h"
#include "VersionInfo.h"
// #include "util/CheatEngine.h"
// #include "util/RegistryUtil.h" // Windows registry, replace with config file
#include "v8xml/WebParser.h"

#include "script/LuaVM.h"
// #include "functionHooks.h" // Windows-specific
// #include "RobloxHooks.h" // Windows-specific
#include "GameVerbs.h"
#include "v8datamodel/GuiService.h"
#include "v8datamodel/TeleportService.h"
#include "VersionInfo.h"

#include "View.h"

// Stub for CheatEngine on Linux
bool vmProtectedDetectCheatEngineIcon() { return false; }

// Stubs for Windows-specific functions
void setupCeLogWatcher() {}

#include "boost/filesystem.hpp"

#include "rbx/Profiler.h"

// Stubs for Windows-specific functions on Linux


LOGGROUP(RobloxWndInit)
LOGGROUP(Network)
FASTFLAGVARIABLE(ReloadSettingsOnTeleport, false)
FASTFLAG(GoogleAnalyticsTrackingEnabled)
DYNAMIC_LOGGROUP(GoogleAnalyticsTracking)
FASTFLAGVARIABLE(DebugUseDefaultGlobalSettings, false)
FASTINTVARIABLE(ValidateLauncherPercent, 0)
FASTINTVARIABLE(BootstrapperVersionNumber, 51261)
FASTINTVARIABLE(RequestPlaceInfoTimeoutMS, 2000)
FASTINTVARIABLE(RequestPlaceInfoRetryCount, 5)
DYNAMIC_FASTINT(JoinInfluxHundredthsPercentage)
FASTINTVARIABLE(InferredCrashReportingHundredthsPercentage, 1000)
FASTFLAGVARIABLE(RwxFailReport, false)
DYNAMIC_FASTFLAGVARIABLE(DontOpenWikiOnClient, false)
DYNAMIC_FASTFLAGVARIABLE(WindowsInferredCrashReporting, false)
FASTFLAG(PlaceLauncherUsePOST)


namespace RBX {

Application::Application()
	: logManager("ROBLOX", ".Client.dmp", ".Client.crashevent")
	, crashReportEnabled(true)
	, hideChat(false)
	, stopLogsCleanup(0)
	, marshaller(NULL)
	, spoofMD5(false)
	, launchMode(SharedLauncher::LaunchMode::Play)
{
	enteredShutdown = 0;
	logsCleanUpThread.reset(new boost::thread(boost::bind(&Application::logsCleanUpHelper, this)));
    mainWindow = nullptr;
}

Application::~Application()
{
	stopLogsCleanup++;
	cleanupPromise.get_future().wait();
}

void Application::logsCleanUpHelper()
{
	std::vector<std::string> folders;
	folders.push_back(simple_logger<char>::get_tmp_path());

	// On Linux, use /tmp or user home
	const char* tmpDir = getenv("TMPDIR");
	if (!tmpDir) tmpDir = "/tmp";
	folders.push_back(std::string(tmpDir));

	auto now = std::chrono::system_clock::now();
	auto maxAge = std::chrono::hours(72);

	for (size_t i = 0; i < folders.size(); ++i) {
		if (stopLogsCleanup == 1)
		{
			break;
		}

		boost::filesystem::path dir(folders[i]);
		if (!boost::filesystem::exists(dir)) continue;

		for (boost::filesystem::directory_iterator it(dir); it != boost::filesystem::directory_iterator(); ++it)
		{
			if (stopLogsCleanup == 1)
			{
				break;
			}

			if (boost::filesystem::is_regular_file(it->path()) && it->path().filename().string().find("RBX") == 0 && it->path().extension() == ".log")
			{
				auto lastWrite = std::chrono::system_clock::from_time_t(boost::filesystem::last_write_time(it->path()));
				if (now - lastWrite > maxAge)
				{
					boost::filesystem::remove(it->path());
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	cleanupPromise.set_value();
}

static HttpFuture fetchJoinScriptAsync(const std::string& url)
{
	if (ContentProvider::isUrl(url) && RBX::Network::isTrustedContent(url.c_str()))
	{
		return HttpAsync::getWithRetries(url, 5);
	}
	else
	{
		// silent error is harder to hack
		return std::shared_future<std::string>(std::async(std::launch::deferred, [](){ return std::string(); }));
	}
}

static std::string readStringValue(shared_ptr<const Reflection::ValueTable> jsonResult, std::string name)
{
    Reflection::ValueTable::const_iterator itData = jsonResult->find(name);
    if (itData != jsonResult->end())
    {
        return itData->second.get<std::string>();
    }
    else
    {
        throw std::runtime_error(RBX::format("Unexpected string result for %s", name.c_str()));
    }
}

static int readIntValue(shared_ptr<const Reflection::ValueTable> jsonResult, std::string name)
{
    Reflection::ValueTable::const_iterator itData = jsonResult->find(name);
    if (itData != jsonResult->end())
    {
        return itData->second.get<int>();
    }
    else
    {
        throw std::runtime_error(RBX::format("Unexpected int result for %s", name.c_str()));
    }
}

Application::RequestPlaceInfoResult Application::requestPlaceInfo(const std::string url, std::string& authenticationUrl, std::string& ticket, std::string& scriptUrl, std::string& message) const
{
	std::string response;
	try
	{
        if (FFlag::PlaceLauncherUsePOST)
        {
            std::istringstream input("");
            RBX::Http(url).post(input, RBX::Http::kContentTypeDefaultUnspecified, false, response);
        }
        else
        {
            RBX::Http(url).get(response);
        }
	}
	catch (RBX::base_exception& e)
	{
		StandardOut::singleton()->printf(MESSAGE_ERROR, "Exception when requesting place info: %s. ", e.what());
	}

	if (!response.empty())
	{
		try
		{
			std::stringstream jsonStream;
			jsonStream << response;
			shared_ptr<const Reflection::ValueTable> jsonResult(rbx::make_shared<const Reflection::ValueTable>());
			bool parseResult = WebParser::parseJSONTable(jsonStream.str(), jsonResult);
			if (parseResult)
			{
				int status = readIntValue(jsonResult, "status");

				if (jsonResult->find("message") != jsonResult->end())
				{
					message = readStringValue(jsonResult, "message");
				}

				if (status == 2)
				{
					authenticationUrl = readStringValue(jsonResult, "authenticationUrl");
					ticket = readStringValue(jsonResult, "authenticationTicket");
					scriptUrl = readStringValue(jsonResult, "joinScriptUrl");
					return SUCCESS;
				}
				else if (status == 6)
					return GAME_FULL;
				else if (status == 10)
					return USER_LEFT;
				else
				{
					// 0 or 1 is not an error - it is a sign that we should wait
					if (status == 0 || status == 1)
						return RETRY;
				}
			}
		}
		catch (std::exception&)
		{
		}
	}

	return FAILED;
}

bool Application::requestPlaceInfo(int placeId, std::string& authenticationUrl, std::string& ticket, std::string& scriptUrl) const
{
	std::string url = RBX::format("%sGame/PlaceLauncher.ashx?request=RequestGame&placeId=%d&isPartyLeader=false&gender=&isTeleport=true",
		GetBaseURL().c_str(), placeId);

	int retries = FInt::RequestPlaceInfoRetryCount;
	while (retries >= 0)
	{
		std::string message;
		if (requestPlaceInfo(url, authenticationUrl, ticket, scriptUrl, message) == SUCCESS)
			return true;
		else
		{
			retries--;
			Sleep(FInt::RequestPlaceInfoTimeoutMS);
		}
	}

	return false;
}

void Application::InferredCrashReportingThreadImpl()
{
	int crash = 0;
#ifdef _WIN32
	if (RBX::RegistryUtil::read32bitNumber("HKEY_CURRENT_USER\\Software\\Roblox Corporation\\ROBLOX\\InferredCrash", crash))
	{
		// We did not had a clean exit last time, report a crash
		RBX::Analytics::InfluxDb::Points points;
		points.addPoint("Session" , crash ? "Crash" : "Success" );
		points.report("Windows-RobloxPlayer-SessionReport-Inferred", FInt::InferredCrashReportingHundredthsPercentage);
		RBX::Analytics::EphemeralCounter::reportCounter(crash ? "Windows-ROBLOXPlayer-Session-Inferred-Crash" : "Windows-ROBLOXPlayer-Session-Inferred-Success", 1, true);
	}

	RBX::RegistryUtil::write32bitNumber("HKEY_CURRENT_USER\\Software\\Roblox Corporation\\ROBLOX\\InferredCrash", 1);
#else
	std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
	std::string crashFile = home + "/.roblox/inferred_crash";
	std::ifstream in(crashFile);
	if (in)
	{
		in >> crash;
		in.close();
		// We did not have a clean exit last time, report a crash
		RBX::Analytics::InfluxDb::Points points;
		points.addPoint("Session" , crash ? "Crash" : "Success" );
		points.report("Linux-RobloxPlayer-SessionReport-Inferred", FInt::InferredCrashReportingHundredthsPercentage);
		RBX::Analytics::EphemeralCounter::reportCounter(crash ? "Linux-ROBLOXPlayer-Session-Inferred-Crash" : "Linux-ROBLOXPlayer-Session-Inferred-Success", 1, true);
	}

	std::ofstream out(crashFile);
	if (out)
	{
		out << 1;
		out.close();
	}
#endif
}

void Application::LaunchPlaceThreadImpl(const std::string& placeLauncherUrl)
{
	std::string authenticationUrl;
	std::string authenticationTicket;
	std::string joinScriptUrl;
	Time startTime = Time::nowFast();

	if (boost::shared_ptr<RBX::Game> game = currentDocument->getGame())
	{
		if (boost::shared_ptr<RBX::DataModel> datamodel = game->getDataModel())
		{
			const std::string reportCategory = "PlaceLauncherDuration";

			// parse request type
			std::string requestType;
			std::string url = placeLauncherUrl;
			safeToLower(url);
			std::string requestArg = "request=";
			size_t requestPos = url.find(requestArg);
			if (requestPos != std::string::npos)
			{
				size_t requestEndPos = url.find("&", requestPos);
				if (requestEndPos == std::string::npos)
					requestEndPos = url.length();

				size_t requestTypeStartPos = requestPos+requestArg.size();
				requestType = url.substr(requestTypeStartPos, requestEndPos-requestTypeStartPos);
			}

			int retries = FInt::RequestPlaceInfoRetryCount;
			RequestPlaceInfoResult res;
			std::string message;
			while (retries >= 0)
			{
				message.clear();
				res = Application::requestPlaceInfo(placeLauncherUrl, authenticationUrl, authenticationTicket, joinScriptUrl, message);

				if (datamodel->isClosed())
					break;

				switch (res)
				{
				case SUCCESS:
					{
						HttpFuture joinScriptResult = fetchJoinScriptAsync(joinScriptUrl);

						currentDocument->startedSignal.connect(boost::bind(&Application::onDocumentStarted, this, _1));
						datamodel->submitTask(boost::bind(&Document::Start, currentDocument.get(), joinScriptResult, launchMode, false, getVRDeviceName()), DataModelJob::Write);

						Analytics::EphemeralCounter::reportStats(reportCategory + "_Success_" + requestType, (Time::nowFast() - startTime).seconds());

						analyticsPoints.addPoint("RequestType", requestType.c_str());
						analyticsPoints.addPoint("Mode", "Protocol");
						analyticsPoints.addPoint("JoinScriptTaskSubmitted", Time::nowFastSec());
						return;
					}
					break;
				case FAILED:
					retries--;
				case RETRY:
				    if (!message.empty())
						currentDocument->SetUiMessage(message);
                    else
                        currentDocument->SetUiMessage("Requesting Server...");

					Sleep(FInt::RequestPlaceInfoTimeoutMS);
					break;
				case GAME_FULL:
					if (!message.empty())
						currentDocument->SetUiMessage(message);
					else
						currentDocument->SetUiMessage("The game you requested is currently full. Waiting for an opening...");
					Sleep(FInt::RequestPlaceInfoTimeoutMS);
					break;
				case USER_LEFT:
					if (!message.empty())
						currentDocument->SetUiMessage(message);
					else
						currentDocument->SetUiMessage("The user has left the game.");
					retries = -1;
					break;
				default:
					break;
				}
			}

			if (res == FAILED)
			{
				Analytics::EphemeralCounter::reportStats(reportCategory + "_Failed_" + requestType, (Time::nowFast() - startTime).seconds());
				if (!message.empty())
					currentDocument->SetUiMessage(message);
				else
					currentDocument->SetUiMessage("Failed to request game, please try again.");
			}
		}
	}
}

void Application::renewLogin(const std::string& authenticationUrl, const std::string& ticket) const
{
	// Simple URL parsing for hostname
	std::string baseUrl = GetBaseURL();
	size_t start = baseUrl.find("://");
	if (start != std::string::npos) start += 3;
	size_t end = baseUrl.find('/', start);
	std::string host = baseUrl.substr(start, end - start);

	AuthenticationMarshallar marshaller(host.c_str());

	marshaller.Authenticate(authenticationUrl.c_str(), ticket.c_str());
}

HttpFuture Application::renewLoginAsync(const std::string& authenticationUrl, const std::string& ticket) const
{
	// Simple URL parsing for hostname
	std::string baseUrl = GetBaseURL();
	size_t start = baseUrl.find("://");
	if (start != std::string::npos) start += 3;
	size_t end = baseUrl.find('/', start);
	std::string host = baseUrl.substr(start, end - start);

	AuthenticationMarshallar marshaller(host.c_str());

	return marshaller.AuthenticateAsync(authenticationUrl.c_str(), ticket.c_str());
}

HttpFuture Application::loginAsync(const std::string& userName, const std::string& passWord) const
{
	std::string loginUrl = RBX::format("%slogin/v1", GetBaseURL().c_str());
	std::string postData = RBX::format("{\"username\":\"%s\", \"password\":\"%s\"}", userName.c_str(), passWord.c_str());

	boost::replace_all(loginUrl, "http", "https");
	boost::replace_all(loginUrl, "www", "api");
	HttpPostData d(postData, Http::kContentTypeApplicationJson, false);
	return HttpAsync::post(loginUrl,d);
}

// Create a crash dump (which results in an upload of logs next time the process
// executes).
void Application::UploadSessionLogs()
{
	if(!logManager.CreateFakeCrashDump())
	{
#ifdef _WIN32
		MessageBoxA(GetHwnd(mainWindow), "Failed to upload logs",
			"Warning", MB_OK | MB_ICONWARNING);
#else
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Warning", "Failed to upload logs", mainWindow);
#endif
	} else {

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "ROBLOX", "Logfiles will be uploaded next time you start ROBLOX. Please restart ROBLOX now.", mainWindow);
	}
}

void Application::OnHelp()
{
	if (!DFFlag::DontOpenWikiOnClient)
	{
#ifdef _WIN32
		ShellExecute(GetHwnd(mainWindow), "open", "rundll32.exe",
			"url.dll,FileProtocolHandler https://developer.roblox.com/", NULL, SW_SHOWDEFAULT);
#else
		system("xdg-open https://developer.roblox.com/");
#endif
	}
}

// Removed OnGetMinMaxInfo as it's Windows-specific


void Application::shareWindow(SDL_Window* window)
{
	// On Linux, perhaps use a file or shared memory, but for simplicity, skip
	FASTLOG1(FLog::RobloxWndInit, "Sharing window handle", 0);
}

static void doMachineIdCheck(Application* app, FunctionMarshaller* marshaller, SDL_Window* window) {
	if (MachineIdUploader::uploadMachineId(GetBaseURL().c_str()) ==
			MachineIdUploader::RESULT_MachineBanned) {

		boost::this_thread::sleep(boost::posix_time::milliseconds(3000));
		marshaller->Execute(boost::bind(&Application::AboutToShutdown, app));
		marshaller->Execute(boost::bind(&Application::Shutdown, app));

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ROBLOX", MachineIdUploader::kBannedMachineMessage, window);
		// On SDL, close the window
		SDL_Event event;
		event.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&event);
	}
}

// Initializes the application.
bool Application::Initialize(SDL_Window* window, const std::string& modulePath)
{
	mainWindow = window;

	if (!LoadAppSettings(modulePath))
		return false;

	{
		// set up external analytics reporting variables
		// On Linux, use a simple version or from config
		std::string version = "1.0.0"; // Placeholder

		// For location, use locale
		std::locale loc("");
		std::string location = "Unknown";

		RBX::Analytics::setReporter("Linux Player");
		RBX::Analytics::setLocation(location);
		RBX::Analytics::setAppVersion(version);
	}

	initializeLogger();
	Game::globalInit(false);

	HttpFuture authenticationResult;
	if (vm.count("userName") > 0 && vm.count("passWord") > 0)
	{
		authenticationResult = loginAsync(vm["userName"].as<std::string>(), vm["passWord"].as<std::string>());
		authenticationResult.wait();
	}

	if(DFFlag::WindowsInferredCrashReporting) // Note: flag name is Windows, but use for Linux too
	{
		reportingThread.reset(new boost::thread(RBX::thread_wrapper(boost::bind(&Application::InferredCrashReportingThreadImpl, this), "Inferred CrashReporting Thread")));
	}

	analyticsPoints.addPoint("BaseInit", Time::nowFastSec());
	double baseInitTime = Time::nowFast().timestampSeconds() * 1000;

	// Determine URLs required to start the game
    std::string authenticationUrl;
    std::string authenticationTicket;
    std::string scriptUrl;
	bool scriptIsPlaceLauncher = false;

	// Load the game using place id from command-line or auth information
	if (vm.count("id") > 0)
	{
		int id = vm["id"].as<int>();

		bool placeFound = requestPlaceInfo(id, authenticationUrl, authenticationTicket, scriptUrl);

        if (!placeFound)
            return false;
	}
	else if (vm.count("authenticationUrl") > 0 && vm.count("authenticationTicket") > 0 && vm.count("joinScriptUrl") > 0)
	{
		authenticationUrl = vm["authenticationUrl"].as<std::string>();
		authenticationTicket = vm["authenticationTicket"].as<std::string>();
		scriptUrl = vm["joinScriptUrl"].as<std::string>();

		if (vm.find("browserTrackerId") != vm.end())
		{
			Stats::setBrowserTrackerId(vm["browserTrackerId"].as<std::string>());
			Stats::reportGameStatus("AppStarted");

			TeleportService::SetBrowserTrackerId(vm["browserTrackerId"].as<std::string>());
		}

		std::string lowerScriptUrl = scriptUrl;
		std::transform(lowerScriptUrl.begin(), lowerScriptUrl.end(), lowerScriptUrl.begin(), ::tolower);
		if (lowerScriptUrl.find("launchplace") != std::string::npos)
		{
			launchMode = SharedLauncher::LaunchMode::Play;
			scriptIsPlaceLauncher = true;
		}
	}

    // Fetch authentication and join script in parallel
	if (!authenticationResult.valid())
		authenticationResult = renewLoginAsync(authenticationUrl, authenticationTicket);

	HttpFuture joinScriptResult;
	if (!scriptIsPlaceLauncher)
		joinScriptResult = fetchJoinScriptAsync(scriptUrl);

	// Collect the MD5 hash if not spoofing in a debug build (note: no fast flags are defined here!)
	if (!spoofMD5)
		DataModel::hash = CollectMd5Hash(moduleFilename);

    	// Check if the user is running CheatEngine (note: no fast flags are defined here!)
    	bool cheatEngine = vmProtectedDetectCheatEngineIcon();

        // Check for Sandboxie. (On Linux, perhaps check for similar)
        bool sandboxie = false; // Placeholder

        	// LuaSecureDouble::initDouble(); // Windows-specific

        // make our code have no rwx sections (this might not work on some computers?)
        	// unsigned int sizeDiff = protectVmpSections(); // Windows-specific

        // DEP not applicable on Linux

	// Reset synchronized flags, they should be set by the server
	FLog::ResetSynchronizedVariablesState();

	{
		bool useCurl = rand() % 100 < RBX::ClientAppSettings::singleton().GetValueHttpUseCurlPercentageWinClient();
		FASTLOG1(FLog::Network, "Using CURL = %d", useCurl);

		//Earlier renewLoginAsync request happened with curl, now if curl is disabled we need to renew the WinInet session
		if (!useCurl)
		{
			RBX::Http::SetUseCurl(false);
			authenticationResult = renewLoginAsync(authenticationUrl, authenticationTicket);
		}

        Http::SetUseStatistics(true);
	}

	// Read global settings (shared between studio and player)
	if (!FFlag::DebugUseDefaultGlobalSettings)
    {
        // Use separate settings for Client to avoid conflicts with Studio
        boost::filesystem::path settingsDir = FileSystem::getUserDirectory(true, DirAppData);
        if (!settingsDir.empty())
        {
            boost::filesystem::path clientSettingsFile = settingsDir / "ClientGlobalSettings_13.xml";
            GlobalAdvancedSettings::singleton()->setSaveTarget(clientSettingsFile.string());
            GlobalAdvancedSettings::singleton()->loadState(clientSettingsFile.string());
        }
        else
        {
            GlobalAdvancedSettings::singleton()->loadState("");
        }
    }

	{
		RBX::Security::Impersonator impersonate(RBX::Security::RobloxGameScript_);
		RBX::GlobalBasicSettings::singleton()->loadState(globalBasicSettingsPath);
	}

#if !defined(RBX_STUDIO_BUILD)
    // hookApi(); // Windows-specific
    // VEH hooks not applicable on Linux
    	// setupCeLogWatcher(); // Windows-specific
#endif

	initializeCrashReporter();

#if defined(LOVE_ALL_ACCESS) || defined(_DEBUG) || defined(_NOOPT)
    StandardOut::singleton()->printf(MESSAGE_ERROR, "Cheat engine%s detected.", cheatEngine ? "" : " not");
#elif !defined(RBX_STUDIO_BUILD)
    DataModel::sendStats |= HATE_CHEATENGINE_OLD * cheatEngine;
    if (cheatEngine)
    {
        RBX::Tokens::sendStatsToken.addFlagSafe(HATE_CHEATENGINE_OLD);
    }
#endif // LOVE_ALL_ACCESS || _DEBUG || NoOpt
#if !defined(LOVE_ALL_ACCESS) && !defined(RBX_STUDIO_BUILD) && !defined(_DEBUG) && !defined(_NOOPT)
    DataModel::sendStats |= HATE_INVALID_ENVIRONMENT * sandboxie;
#endif

	setWindowFrame(); // Obfuscated name to hide functionality for security reasons
	uploadCrashData(false);
	shareWindow(window);

	// Ensure a single instance (on Linux, perhaps use a lock file)
	singleRunningInstance.reset(new boost::thread(boost::bind(&Application::waitForNewPlayerProcess, this, window)));

	profanityFilter = ProfanityFilter::getInstance();

	Profiler::onThreadCreate("Main");

	// initialize the TaskScheduler
	TaskScheduler::singleton().setThreadCount(TaskSchedulerSettings::singleton().getThreadPoolConfig());

	if (RBX::ClientAppSettings::singleton().GetValueGoogleAnalyticsInitFix())
	{
		RBX::Analytics::GoogleAnalytics::lotteryInit(RBX::ClientAppSettings::singleton().GetValueGoogleAnalyticsAccountPropertyIDPlayer(),
			RBX::ClientAppSettings::singleton().GetValueGoogleAnalyticsLoadPlayer());
	}
	else
	{
		int lottery = rand() % 100;
		FASTLOG1(DFLog::GoogleAnalyticsTracking, "Google analytics lottery number = %d", lottery);
		// initialize google analytics
		if (FFlag::GoogleAnalyticsTrackingEnabled && (lottery < RBX::ClientAppSettings::singleton().GetValueGoogleAnalyticsLoadPlayer()))
		{
			RBX::RobloxGoogleAnalytics::setCanUseAnalytics();

			RBX::RobloxGoogleAnalytics::init(RBX::ClientAppSettings::singleton().GetValueGoogleAnalyticsAccountPropertyIDPlayer(),
				RBX::ClientAppSettings::singleton().GetValueGoogleAnalyticsThreadPoolMaxScheduleSize());
		}
	}

	RobloxGoogleAnalytics::trackUserTiming(GA_CATEGORY_GAME, GA_CLIENT_START, baseInitTime, "Base Init");
	RobloxGoogleAnalytics::trackUserTiming(GA_CATEGORY_GAME, GA_CLIENT_START, Time::nowFast().timestampSeconds() * 1000, "Load ClientAppSettings");
	analyticsPoints.addPoint("SettingsLoaded", Time::nowFastSec());

	// On Linux, no GetProfileString, use a config file
	std::string strProfile = "0"; // Placeholder

	RBX::postMachineConfiguration(GetBaseURL().c_str(), atoi(strProfile.c_str()));

	RobloxGoogleAnalytics::trackUserTiming(GA_CATEGORY_GAME, GA_CLIENT_START, Time::nowFast().timestampSeconds() * 1000, "Post machine info");

    // TaskScheduler::singleton().add( shared_ptr<TaskScheduler::Job>( new VerifyConnectionJob() ) ); // Not defined

    // reporting for odd issues with the vmprotect sections.
    // if (FFlag::RwxFailReport && sizeDiff > 4096)
    // {
    //     std::stringstream msg;
    //     msg << std::hex << sizeDiff;
    //     RBX::Analytics::GoogleAnalytics::trackEvent(GA_CATEGORY_GAME, "VmpInfo", msg.str().c_str());
    // }

	if (!scriptIsPlaceLauncher)
	{
		// Create new document and start the game
		StartNewGame(window, joinScriptResult, false);

		authenticationResult.wait();
	}
	else
	{
		InitializeNewGame(window);
		currentDocument->SetUiMessage("Requesting Server...");

		// make sure we are authenticated before launching place
		authenticationResult.wait();

		Stats::reportGameStatus("AcquiringGame");
		launchPlaceThread.reset(new boost::thread(RBX::thread_wrapper(boost::bind(&Application::LaunchPlaceThreadImpl, this, scriptUrl), "Launch Place Thread")));
		analyticsPoints.addPoint("PlaceLauncherThreadStarted", Time::nowFastSec());
	}

	marshaller = FunctionMarshaller::GetWindow();
    teleporter.initialize(this, marshaller);

    // this needs to happen after we've hit the login authenticator
    // and after the document has been completely set up by StartNewGame
    boost::thread t(boost::bind(&doMachineIdCheck, this, marshaller, window));

	RobloxGoogleAnalytics::trackUserTiming(GA_CATEGORY_GAME, GA_CLIENT_START, Time::nowFast().timestampSeconds() * 1000, "Game started");

	// delay setting started event if in protocol handler mode, bootstrapper dialog needs to stay up until this window is shown
	if (!scriptIsPlaceLauncher)
	{
		// On Linux, no ATL::CEvent, perhaps use a file or skip
	}

    // Wait to display the window for video preroll
    if (ClientAppSettings().singleton().GetValueAllowVideoPreRoll() && !waitEventName.empty())
        showWindowAfterEvent.reset(new boost::thread(boost::bind(&Application::waitForShowWindow, this, ClientAppSettings().singleton().GetValueVideoPreRollWaitTimeSeconds() * 1000)));
    else
        showWindowAfterEvent.reset(new boost::thread(boost::bind(&Application::waitForShowWindow, this, 0)));

	bool startBootstrapperValidationThread = rand() % 100 < FInt::ValidateLauncherPercent;
	if (startBootstrapperValidationThread)
		validateBootstrapperVersionThread.reset(new boost::thread(boost::bind(&Application::validateBootstrapperVersion, this)));

	return true;
}

	// Get settings from AppSettings.xml
bool Application::LoadAppSettings(const std::string& modulePath)
{
	try {
		// Get the file names for the application
		moduleFilename = modulePath;
		size_t found = moduleFilename.find_last_of("/\\");
		std::string settingsFileName = moduleFilename.substr(0, found + 1) + "AppSettings.xml";

		// Declare XML elements to be used by parser
		const XmlTag& tagBaseUrl = Name::declare("BaseUrl");
		const XmlTag& tagSilentCrashReport = Name::declare("SilentCrashReport");
		const XmlTag& tagContentFolder = Name::declare("ContentFolder");
		const XmlTag& tagHideChatWindow = Name::declare("HideChatWindow");

		// Parse the xml
		std::ifstream fileStream(settingsFileName.c_str());
		TextXmlParser parser(fileStream.rdbuf());
		std::auto_ptr<XmlElement> root;
		root = parser.parse();

		// Crash reporting.
#ifdef _WIN32
		int value = 1;
		if (RBX::RegistryUtil::read32bitNumber("HKEY_CURRENT_USER\\Software\\Roblox Corporation\\ROBLOX\\CrashReport", value))
		{
			crashReportEnabled = (0 != value);
		}
#else
		std::string configFile = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + "/.roblox/config";
		std::ifstream configIn(configFile);
		if (configIn)
		{
			std::string line;
			while (std::getline(configIn, line))
			{
				if (line.find("CrashReport=1") != std::string::npos)
					crashReportEnabled = true;
			}
		}
#endif

		// Disable crash reporting on debug builds.
		#ifdef _DEBUG
		crashReportEnabled = false;
		#endif // _DEBUG

		// Silent Crash Report
		const XmlElement* xmlSilentCrashReport = root->findFirstChildByTag(tagSilentCrashReport);
		int valSilentCrashReport = 1;
		if (xmlSilentCrashReport != NULL) {
			xmlSilentCrashReport->getValue(valSilentCrashReport);
		}
#ifdef _WIN32
		if (0 != valSilentCrashReport) {
			valSilentCrashReport = 0;
			int value = 0;
			if (RBX::RegistryUtil::read32bitNumber("HKEY_CURRENT_USER\\Software\\Roblox Corporation\\ROBLOX\\SilentCrashReport", value)) {
				valSilentCrashReport = (0 == value) ? 0 : 1;
			}
		}
#endif
		RobloxCrashReporter::silent = (valSilentCrashReport != 0);

		// BaseURL
		const XmlElement* xmlBaseUrl = root->findFirstChildByTag(tagBaseUrl);
		std::string valBaseUrl;
		if (xmlBaseUrl && xmlBaseUrl->getValue(valBaseUrl)) {
			if (!valBaseUrl.empty() && (*valBaseUrl.rbegin()) != '/') {
				valBaseUrl.append("/");
			}
			SetBaseURL(valBaseUrl);
		}

		// Content folder
		const XmlElement* xmlContentFolder = root->findFirstChildByTag(tagContentFolder);
		std::string valContentFolder;
		if (xmlContentFolder && xmlContentFolder->getValue(valContentFolder)) {
			boost::filesystem::path contentPath(valContentFolder);
			if (!contentPath.is_absolute()) {
				boost::filesystem::path exeDir = boost::filesystem::path(moduleFilename).parent_path();
				contentPath = exeDir / contentPath;
				valContentFolder = contentPath.string();
			}
			ContentProvider::setAssetFolder(valContentFolder.c_str());
		}

		// Hide chat window
		const XmlElement* xmlHideChatWindow = root->findFirstChildByTag(tagHideChatWindow);
		if (xmlHideChatWindow != NULL) {
			int hide;
			if (xmlHideChatWindow->getValue(hide)) {
				hideChat = (hide != 0) ? true : false;
			}
		}

		return true;
	}
	catch(const std::exception& e)
	{
		handleError(e);
		return false;
	}
}

void Application::AboutToShutdown() {
	int expected = 0;
	enteredShutdown.compare_exchange_strong(expected, 1);

    // this will kill all misbehaving (hanging) scripts
    if (currentDocument)
        currentDocument->PrepareShutdown();

    if (mainView)
	    mainView->AboutToShutdown();
}

// Free resources and perform cleanup operations before Application
// destructor is called.
void Application::Shutdown()
{
    shutdownVerbs();

    if (mainView)
    {
        mainView->Stop();
        mainView.reset();
    }

    // kill thread that prevents multiple players.
    if (singleRunningInstance) {
        singleRunningInstance->join();
    }
    singleRunningInstance.reset();

    // If the showWindowAfterEvent is still pending, then join it so that it
	// won't try to show window while we are shutting down currentDocument.
	if (showWindowAfterEvent) {
		showWindowAfterEvent->join();
	}

	showWindowAfterEvent.reset();

    if (currentDocument)
    {
        currentDocument->Shutdown();
        currentDocument.reset();
    }

	RBX::GlobalBasicSettings::singleton()->saveState();
	// DE7393 - Do not save GlobalAdvancedSettings, it should be saved only from Studio
	// ENABLED for ROBLOX Player to persist settings
	RBX::GlobalAdvancedSettings::singleton()->saveState();
	Game::globalExit();

	// TODO: This is where we would join with the game release thread to make
	// sure the game object has been totally torn down in the case of
	// simultaneous teardown and load.

	if (marshaller) {
		FunctionMarshaller::ReleaseWindow(marshaller);
	}

	// Write to file instead of registry
	std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
	boost::filesystem::create_directories(home + "/.roblox");
	std::ofstream out(home + "/.roblox/inferred_crash");
	if (out) {
		out << 0;
		out.close();
	}

}

// Parses command line arguments.
bool Application::ParseArguments(int argc, char* argv[])
{
	try
	{
		po::options_description desc("Options");
		desc.add_options()
			("help,?", "produce help message")
			("globalBasicSettingsPath,g", po::value<std::string>(), "path to GlobalBasicSettings_(n).xml")
			("version,v", "print version string")
			("id", po::value<int>(), "id of the place to join")
			("content,c", po::value<std::string>(),
			"path to the content directory")
			("authenticationUrl,a", po::value<std::string>(), "authentication url from server")
			("authenticationTicket,t", po::value<std::string>(), "game session ticket from server")
			("joinScriptUrl,j", po::value<std::string>(), "url of launch script from server")
			("browserTrackerId,b", po::value<std::string>(), "browser tracking id from website")
			("waitEvent,w", po::value<std::string>(), "window is invisible until this named event is signaled")
			("API", po::value<std::string>(), "output API file")
			("dmp,d", "upload crash dmp")
			("play", "specifies the launching of a game")
			("app", "specifies the launching of an app")
			("fast", "uses fast startup path")

			#if defined(LOVE_ALL_ACCESS) || defined(_DEBUG) || defined(_NOOPT)
			("userName", po::value<std::string>(), "Your ROBLOX UserName")
			("passWord", po::value<std::string>(), "Your ROBLOX Password")
			("md5", po::value<std::string>(),
			"md5 hash to spoof (debug build only)")
			#endif // defined(LOVE_ALL_ACCESS) || defined(_DEBUG) || defined(_NOOPT)

			;

		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);

		if (vm.count("help"))
		{
			std::basic_stringstream<char> options;
			desc.print(options);
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "ROBLOX", options.str().c_str(), NULL);
			return false;
		}

		if (vm.count("API") > 0)
		{
			std::string fileName = vm["API"].as<std::string>();
			std::ofstream stream(fileName.c_str());
			RBX::Reflection::Metadata::writeEverything(stream);
			return false;
		}
		if (vm.count("dmp") > 0)
				if (vm.count("dmp") > 0)
				{
					// initialize crash reporter here so we can catch any crash that might happen during upload
					initializeCrashReporter();
					logManager.EnableImmediateCrashUpload(false);

					LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, "Uploading dmp");

					std::string dmpHandlerUrl = GetDmpUrl(GetBaseURL(), true);
		#ifdef _WIN32
					dumpErrorUploader.reset(new DumpErrorUploader(false, "WindowsClient"));
		#else
					dumpErrorUploader.reset(new DumpErrorUploader(false, "LinuxClient"));
		#endif
					dumpErrorUploader->Upload(dmpHandlerUrl);
					return false;
				}

        // Note, this setting is also used in an obsfucated way in --waitEvent.
		if (vm.count("globalBasicSettingsPath"))
		{
			globalBasicSettingsPath = vm["globalBasicSettingsPath"].as<std::string>();
		}

		if (vm.count("version"))
		{
#ifdef _WIN32
			// Simplified version for MinGW
			std::string versionNumber = "1.0.0 (Win32)";
			LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, versionNumber.c_str());
			MessageBoxA(NULL, versionNumber.c_str(), "Version", MB_OK);
			versionNumber.append("\n");
			OutputDebugString( versionNumber.c_str() );
#else
			std::string versionNumber = "1.0.0"; // Placeholder
			LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, versionNumber.c_str());
			std::cout << versionNumber << std::endl;
#endif
			return false;
		}

		if (vm.count("content"))
		{
			std::string contentDir(vm["content"].as<std::string>());
			ContentProvider::setAssetFolder(contentDir.c_str());
		}

		#if defined(LOVE_ALL_ACCESS) || defined(_DEBUG) || defined(_NOOPT)
		if (vm.count("md5"))
		{
			spoofMD5 = true;
			DataModel::hash = vm["md5"].as<std::string>();
			LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, std::string("spoofing MD5 hash as ").append(DataModel::hash).c_str());
		}
		#endif // defined(LOVE_ALL_ACCESS) || defined(_DEBUG) || defined(_NOOPT)

        if (vm.count("waitEvent"))
        {
            waitEventName = vm["waitEvent"].as<std::string>();
            // Obfuscated code removed for Linux
        }

		// used to determine how we will initialize datamodel
		if (vm.count("play"))
			launchMode = SharedLauncher::LaunchMode::Play;

		return true;
	}
	catch(const std::exception& e)
	{
		LogManager::ReportEvent(EVENTLOG_ERROR_TYPE, RBX::format("Command-line args: %s", argv).c_str());
		handleError(e);
		return false;
	}
}

// Posts an SDL event.


// Resizes the viewport when the window is resized.
void Application::OnResize(int w, int h)
{
    if (mainView)
	    mainView->OnResize(w, h);
}



void Application::initializeLogger()
{
	StandardOut::singleton()->messageOut.connect(
		&Application::onMessageOut);
}

// Inform client to tell server to disconnect game if we are not a signed
void Application::setWindowFrame()
{
    // do nothing
}

void Application::initializeCrashReporter()
{
	if (!crashReportEnabled)
		return;

	logManager.WriteCrashDump();
	logManager.NotifyFGThreadAlive();

	DebugSettings& ds = DebugSettings::singleton();
	if (ds.getErrorReporting() == DebugSettings::DontReport)
		return;

	dumpErrorUploader.reset(new DumpErrorUploader(true, "LinuxClient"));
	dumpErrorUploader->InitCrashEvent(GetDmpUrl(GetBaseURL(), true), logManager.getCrashEventName());
}


void Application::uploadCrashData(bool userRequested)
{
	if (!crashReportEnabled)
		return;

	// Assume network is alive on Linux
	std::string dmpHandlerUrl = GetDmpUrl(::GetBaseURL(), true);
	if (logManager.hasCrashLogs(".dmp"))
		switch (RBX::DebugSettings::singleton().getErrorReporting())
	{
		case RBX::DebugSettings::DontReport:
			break;
		case RBX::DebugSettings::Prompt:
			{
				// On Linux, prompt via console or SDL
				std::cout << "Upload crash report? (y/n): ";
				char response;
				std::cin >> response;
				if (response == 'y' || response == 'Y')
					dumpErrorUploader->Upload(dmpHandlerUrl);
				else
					logManager.gatherCrashLogs();
			}
			break;
		case RBX::DebugSettings::Report:
			dumpErrorUploader->Upload(dmpHandlerUrl);
			break;
	}
}

void Application::handleError(const std::exception& e)
{
	std::string message(e.what());
	message.append("\n");
	std::cerr << message;
	LogManager::ReportException(e);
	if (!RobloxCrashReporter::silent) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ROBLOX", e.what(), mainWindow);
	}
}

void Application::waitForNewPlayerProcess(SDL_Window* window)
{
	// On Linux, single instance check not implemented
}



void Application::waitForShowWindow(int delay)
{
	bool showWindow = true;
	if (delay != 0) {
		// On Linux, wait for a file or something, but simplify
		LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE,
			RBX::format("Waiting for event before showing window: %s", waitEventName.c_str()).c_str());

		weak_ptr<RBX::Soundscape::SoundService> weakSoundService;

		if (boost::shared_ptr<RBX::Game> game = currentDocument->getGame())
		{
			if (RBX::Soundscape::SoundService* soundService = ServiceProvider::create<RBX::Soundscape::SoundService>(game->getDataModel().get()))
			{
				weakSoundService = weak_from(soundService);
				soundService->muteAllChannels(true);
			}
		}

		// Wait for delay
		std::this_thread::sleep_for(std::chrono::milliseconds(delay));

		if (shared_ptr<RBX::Soundscape::SoundService> soundService = weakSoundService.lock())
		{
			RBX::DataModel::LegacyLock lock(RBX::DataModel::get(soundService.get()), DataModelJob::Write);
			soundService->muteAllChannels(false);
		}
	}

	if (showWindow) {
		if (mainView)
			mainView->ShowWindow();

		// On Linux, no ATL::CEvent
	}
}

void Application::validateBootstrapperVersion()
{
	boost::filesystem::path dir = RBX::FileSystem::getUserDirectory(false, RBX::DirExe);
	boost::filesystem::path launcherName = "RobloxPlayerLauncher.exe";
	boost::filesystem::path launcherPath = dir / launcherName;

	CVersionInfo vi;
	if (true)
	{
		bool needUpdate = false;
		std::string v = vi.GetFileVersionAsDotString();
		int pos = v.rfind(".");
		if (pos != std::string::npos)
		{
			std::string n = v.substr(pos+1);
			if (atoi(n.c_str()) < FInt::BootstrapperVersionNumber)
				needUpdate = true;
		}

		if (!needUpdate)
			return;

		try
		{
			std::string version;
			std::string installHost;
			std::string baseUrl = GetBaseURL();

			pos = baseUrl.find("www");
			if (pos == std::string::npos)
				return;

			// from www.roblox.com or www.gametest1.robloxlabs.com to setup.roblox.com or setup.gametest1.robloxlabs.com, etc...
			installHost = "http://setup" + baseUrl.substr(pos+3);

			{
				Http http(installHost + "version");
				http.get(version);
			}

			if (version.length() > 0)
			{
				std::string downloadUrl = RBX::format("%s%s-%s", installHost.c_str(), version.c_str(), launcherName.c_str());
				Http http(downloadUrl);
				std::string file;
				http.get(file);

				if (!file.length())
					throw std::runtime_error("Invalid download");

				// write file to tmp directory
				boost::filesystem::path tempPath = boost::filesystem::temp_directory_path();
				tempPath /= "rbxtmp";
				std::ofstream outStream(tempPath.c_str(), std::ios_base::out | std::ios::binary);
				outStream.write(file.c_str(), file.length());
				outStream.close();

				// copy file to current directory
				boost::filesystem::path tempDestPath = dir / tempPath.filename();
				boost::filesystem::copy_file(tempPath, tempDestPath, boost::filesystem::copy_options::overwrite_existing);

				// remove previous launcher and rename new one
				boost::system::error_code ec;
				boost::filesystem::remove(launcherPath, ec);

				// try again if failed to remove
				if (ec.value() == boost::system::errc::io_error)
				{
					StandardOut::singleton()->print(MESSAGE_INFO, "validateBootstrapperVersion file remove failed, retrying in 10 seconds");
					boost::this_thread::sleep(boost::posix_time::seconds(10));
					boost::filesystem::remove(launcherPath);
				}

				boost::filesystem::rename(tempDestPath, launcherPath);
			}
		}
		catch (boost::filesystem::filesystem_error& e)
		{
			StandardOut::singleton()->printf(MESSAGE_INFO, "validateBootstrapperVersion File Error: %s (%d)", e.what(), e.code().value());
			RobloxGoogleAnalytics::trackEventWithoutThrottling(GA_CATEGORY_ERROR, "ValidateBootstrapperVersion File Error", e.what());
		}
		catch (RBX::base_exception& e)
		{
			StandardOut::singleton()->printf(MESSAGE_INFO, "validateBootstrapperVersion Error: %s", e.what());
			RobloxGoogleAnalytics::trackEventWithoutThrottling(GA_CATEGORY_ERROR, "ValidateBootstrapperVersion Error", e.what());
		}

	}
	else
		RobloxGoogleAnalytics::trackEventWithoutThrottling(GA_CATEGORY_ERROR, "ValidateBootstrapperVersion Fail Loading Version Info", launcherPath.string().c_str());
}

void Application::onMessageOut(const StandardOutMessage& message)
{
	std::string level;

	switch (message.type)
	{
	case MESSAGE_INFO:
		level = "INFO";
		break;
	case MESSAGE_WARNING:
		level = "WARNING";
		break;
	case MESSAGE_ERROR:
		level = "ERROR";
		break;
	default:
		level.clear();
		break;
	}

	std::ostringstream outputString;
	outputString << level << ": " << message.message << std::endl;

	#ifdef _WIN32
	OutputDebugString(outputString.str().c_str());
	#else
	std::cout << outputString.str();
	#endif
}

void Application::Teleport(const std::string& authenticationUrl,
						   const std::string& ticket,
						   const std::string& scriptUrl)
{
    currentDocument->PrepareShutdown();
    mainView->Stop();
    shutdownVerbs();
    currentDocument->Shutdown();
	currentDocument.reset();


	if (FFlag::ReloadSettingsOnTeleport)
	{
		// remove invalid children from settings()
		RBX::GlobalAdvancedSettings::singleton()->removeInvalidChildren();
		RBX::GlobalBasicSettings::singleton()->removeInvalidChildren();
	}

	{
        HttpFuture authenticationResult = renewLoginAsync(authenticationUrl, ticket);
		HttpFuture joinScriptResult = fetchJoinScriptAsync(scriptUrl);

        // Create new document and start the game
        StartNewGame(mainWindow, joinScriptResult, true);

        authenticationResult.wait();
	}
}

void Application::InitializeNewGame(SDL_Window* window)
{
	LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, "Initializing new game");

	currentDocument.reset(new Document());
	currentDocument->Initialize( window, !hideChat );

    mainView.reset(new View(window));
    mainView->Start(currentDocument->getGame());

    if (boost::shared_ptr<RBX::DataModel> datamodel = currentDocument->getGame()->getDataModel())
    {
    }


    initVerbs();
}

void Application::StartNewGame(SDL_Window* window, HttpFuture& scriptResult, bool isTeleport)
{
	LogManager::ReportEvent(EVENTLOG_INFORMATION_TYPE, "Starting or teleporting to game");

	currentDocument.reset(new Document());
	currentDocument->Initialize( window, !hideChat );

    mainView.reset(new View(window));
    mainView->Start(currentDocument->getGame());

    if (boost::shared_ptr<RBX::DataModel> datamodel = currentDocument->getGame()->getDataModel())
    {
        currentDocument->startedSignal.connect(boost::bind(&Application::onDocumentStarted, this, _1));
        datamodel->submitTask(boost::bind(&Document::Start, currentDocument.get(), scriptResult, launchMode, false, getVRDeviceName()), DataModelJob::Write);
    }

    initVerbs();
}

void Application::initVerbs()
{
    RBXASSERT(currentDocument);

    DataModel* dm = currentDocument->getGame()->getDataModel().get();
    leaveGameVerb.reset(new LeaveGameVerb(*mainView, dm));
    RBX::ViewBase* gfx = mainView->GetGfxView();
    screenshotVerb.reset(new ScreenshotVerb(*currentDocument, gfx, currentDocument->getGame().get()));
    toggleFullscreenVerb.reset(new ToggleFullscreenVerb(*mainView, dm));
}

void Application::shutdownVerbs()
{
    toggleFullscreenVerb.reset();
    leaveGameVerb.reset();
    screenshotVerb.reset();
}

const char* Application::getVRDeviceName()
{
	return mainView ? mainView->GetGfxView()->getVRDeviceName() : 0;
}

void Application::onDocumentStarted(bool isTeleport)
{
	if (!isTeleport)
	{
		analyticsPoints.addPoint("JoinScriptTaskStarted", Time::nowFastSec());
		analyticsPoints.report("ClientLaunch", DFInt::JoinInfluxHundredthsPercentage);
	}
}

void Application::HandleSDLEvent(SDL_Event* event)
{
	if (mainView)
	{
		mainView->HandleSDLEvent(event);
	}
}



}  // namespace RBX
