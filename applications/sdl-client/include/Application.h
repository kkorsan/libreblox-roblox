#pragma once

#include "SDL3/SDL.h"
#include "LogManager.h"
#include "Teleporter.h"
#include "SharedLauncher.h"

#include "rbx/atomic.h"
#include "v8datamodel/FastLogSettings.h"

#include "util/HttpAsync.h"
#include "util/Analytics.h"

#include <boost/program_options.hpp>
#include <atomic>
#include <condition_variable>
#include <future>

// forward declarations
// Removed Windows-specific namespace alias

class CProcessPerfCounter;
class DumpErrorUploader;

namespace RBX {

// more forward declarations
class Game;
class UserInput;
class RenderJob;
class LeaveGameVerb;
class ScreenshotVerb;
class ToggleFullscreenVerb;
class ProfanityFilter;
class FunctionMarshaller;
class Document;
struct StandardOutMessage;
class View;

namespace Tasks { class Sequence; }

class Application {

	enum RequestPlaceInfoResult
	{
		SUCCESS,
		FAILED,
		RETRY,
		GAME_FULL,
		USER_LEFT,
	};

	RBX::Analytics::InfluxDb::Points analyticsPoints;

public:
	Application();
	~Application();

	// Initializes the application.
	bool Initialize(SDL_Window* window, const std::string& modulePath);

	// Load AppSettings.xml
	bool LoadAppSettings(const std::string& modulePath);

	// Notification that Shutdown will be happening soon
	void AboutToShutdown();

	// Free resources and perform cleanup operations before Application
	// destructor is called.
	void Shutdown();

	// Parses command line arguments.
	bool ParseArguments(int argc, char* argv[]);

	// Posts messages the document or view care about (mouse, keyboard, window
	// activation, etc).
	void HandleSDLEvent(SDL_Event* event);

	// Resizes the viewport when the window is resized.
	void OnResize(int w, int h);

	void Teleport(const std::string& authenticationUrl,
		const std::string& ticket,
		const std::string& scriptUrl);

	// Creates a crash dump which results in an upload to the server the next
	// time the process runs.
	void UploadSessionLogs();

	// Show help wiki
	void OnHelp();

	// Named event to wait for before showing the window
	std::string WaitEventName() { return waitEventName; }


private:
	boost::scoped_ptr<boost::thread> launchPlaceThread;
	boost::scoped_ptr<boost::thread> reportingThread;
	void LaunchPlaceThreadImpl(const std::string& placeLauncherUrl);
	void InferredCrashReportingThreadImpl();

	void InitializeNewGame(SDL_Window* window);

	void StartNewGame(SDL_Window* window, HttpFuture& scriptResult, bool isTeleport);

	void initializeLogger();
	void setWindowFrame();
	void initializeCrashReporter();
	void uploadCrashData(bool userRequested);
	void handleError(const std::exception& e);
	bool requestPlaceInfo(int placeId, std::string& authenticationUrl,
		std::string& ticket,
		std::string& scriptUrl) const;
	RequestPlaceInfoResult requestPlaceInfo(const std::string url, std::string& authenticationUrl,
		std::string& ticket,
		std::string& scriptUrl, std::string& message) const;
	void renewLogin(const std::string& authenticationUrl,
		const std::string& ticket) const;
	HttpFuture renewLoginAsync(const std::string& authenticationUrl,
		const std::string& ticket) const;

	HttpFuture loginAsync(const std::string& userName, const std::string& passWord) const;

	void shareWindow(SDL_Window* window);

	const char* getVRDeviceName();

	std::atomic<int> enteredShutdown;
    SDL_Window* mainWindow;

	// Removed Windows-specific HANDLE and ATL types
	std::atomic<int> stopLogsCleanup;
	std::promise<void> cleanupPromise;
	boost::scoped_ptr<boost::thread> logsCleanUpThread;
	void logsCleanUpHelper();

	// Thread to prevent multiple instances of WindowsPlayer from running
	// simultaneously
	void waitForNewPlayerProcess(SDL_Window* window);
	void waitForShowWindow(int delay);
	void validateBootstrapperVersion();

	// Sends messages to the debugger for display.
	static void onMessageOut(const StandardOutMessage& message);

	// Gets version number from .rc and returns display-friendly representation
	std::string getversionNumber();

	// determines what kind of game mode we are in
	SharedLauncher::LaunchMode launchMode;

    // Application owns the views
    boost::scoped_ptr<View>        mainView;

    // Removed duplicate mainWindow declaration

	// Command-line arguments
	boost::program_options::variables_map vm;

	// Filename and path
	std::string moduleFilename;

	// Settings (e.g. GlobalBasicSettings_10.xml)
	std::string globalBasicSettingsPath;

	// If not empty string this is the name of a named event the
	std::string waitEventName;

	// Application owns the Document
	boost::scoped_ptr<Document> currentDocument;

	// The crash report components.
	bool crashReportEnabled, hideChat;
	MainLogManager logManager;
	boost::scoped_ptr<DumpErrorUploader> dumpErrorUploader;

	boost::shared_ptr<CProcessPerfCounter> processPerfCounter;
	boost::shared_ptr<ProfanityFilter> profanityFilter;

	boost::scoped_ptr<boost::thread> singleRunningInstance;
	boost::scoped_ptr<boost::thread> showWindowAfterEvent;
	boost::scoped_ptr<boost::thread> validateBootstrapperVersionThread;

	Teleporter teleporter;

	FunctionMarshaller* marshaller;
	bool spoofMD5; // Only used in DEBUG / NOOPT builds

    boost::scoped_ptr<ToggleFullscreenVerb> toggleFullscreenVerb;
    boost::scoped_ptr<LeaveGameVerb> leaveGameVerb;
    boost::scoped_ptr<ScreenshotVerb> screenshotVerb;

    void initVerbs();
    void shutdownVerbs();

    void onDocumentStarted(bool isTeleport);

};

}  // namespace RBX
