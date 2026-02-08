#pragma once

#include "WinTypes.h"
#include "rbx/Log.h"
#include "rbx/Boost.hpp"
#include "util/Exception.h"
#include <boost/filesystem.hpp>
#include "network/CrashReporter.h"
#include "boost/scoped_ptr.hpp"
#include <vector>
#include "rbx/threadsafe.h"
#include "rbx/atomic.h"



class LogManager
{
	RBX::Log* log;
	static bool logsEnabled;
	static rbx::atomic<int> nextThreadID;
protected:
	int threadID;
	std::string name;
	static class MainLogManager* mainLogManager;
public:
	RBX::Log* getLog();

	static MainLogManager* getMainLogManager();

	static void ReportException(std::exception const& exp);
	static void ReportLastError(const char* message);
	static void ReportEvent(int type, const char* message);
	static void ReportEvent(int type, const char* message, const char* fileName, int lineNumber);

	const boost::filesystem::path& GetLogPath() const;

	virtual ~LogManager();

	virtual std::string getLogFileName() = 0;

protected:
	LogManager(const char* name);
};


class RobloxCrashReporter : public CrashReporter
{
public:
	static bool silent;
	RobloxCrashReporter(const char* outputPath, const char* appName, const char* crashExtention);
	LONG ProcessException(struct _EXCEPTION_POINTERS *info, bool noMsg);
protected:
	/*override*/ void logEvent(const char* msg);
};

class MainLogManager
	: public RBX::ILogProvider
	, public LogManager
{
	boost::scoped_ptr<RobloxCrashReporter> crashReporter;
	std::vector<RBX::Log*> fastLogChannels;
	static RBX::mutex fastLogChannelsLock;
	const char* crashExtention;
	const char* crashEventExtention;
public:
	MainLogManager(LPCTSTR productName, const char* crashExtention, const char* crashEventExtention);	// used for main thread
	~MainLogManager();

	RBX::Log* provideLog();
	virtual std::string getLogFileName();
	std::string getFastLogFileName(FLog::Channel channelId);
	std::string MakeLogFileName(const char* postfix);

	bool hasErrorLogs() const;
	bool hasCrashLogs(std::string extension) const;
	// Move crash logs to the archive and return the file paths
	std::vector<std::string> gatherCrashLogs();
	std::vector<std::string> gatherAssociatedLogs(const std::string& filenamepattern);
	void CullLogs(const char* folder, int filesRemaining);

	std::vector<std::string> gatherScriptCrashLogs();

	void WriteCrashDump();

	// triggers upload of log files on next start.
	bool CreateFakeCrashDump();

	void NotifyFGThreadAlive(); // for deadlock reporting. call every second.
	void DisableHangReporting();

	void EnableImmediateCrashUpload(bool enabled);

	// returns HEX string that will be part of all the log/dumps output for this session.
	std::string getSessionId();

	std::string getCrashEventName();

	static void fastLogMessage(FLog::Channel id, const char* message);

	enum GameState
	{
		UN_INITIALIZED = 0,
		IN_GAME,
		LEAVE_GAME
	};
	GameState getGameState() { return gameState; }
	void setGameLoaded() { gameState = GameState::IN_GAME; }
	void setLeaveGame()  { gameState = GameState::LEAVE_GAME; };

private:

	GameState gameState;
	std::string guid;
	static bool handleDebugAssert(
		const char* expression,
		const char* filename,
		int         lineNumber
	);
	static bool handleFailure(
		const char* expression,
		const char* filename,
		int         lineNumber
	);

	static bool handleG3DFailure(
		const char* _expression,
		const std::string& message,
		const char* filename,
		int lineNumber,
		/*bool& ignoreAlways,*/
		bool useGuiPrompt);

	static bool handleG3DDebugAssert(
		const char* _expression,
		const std::string& message,
		const char* filename,
		int lineNumber,
		/*bool& ignoreAlways,*/
		bool useGuiPrompt);

	std::vector<std::string> getRecentCseFiles();
};

class ThreadLogManager
	: public LogManager
{
	ThreadLogManager();
public:
	static ThreadLogManager* getCurrent();
	virtual ~ThreadLogManager();
protected:
	virtual std::string getLogFileName();
};
