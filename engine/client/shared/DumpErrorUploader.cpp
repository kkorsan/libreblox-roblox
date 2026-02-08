/* Copyright 2003-2007 ROBLOX Corporation, All Rights Reserved */

#include <boost/interprocess/errors.hpp>
#ifdef _WIN32
#include "stdafx.h"
#undef min
#undef max
#endif

#include "rbx/Log.h"

#include "DumpErrorUploader.h"
#include "LogManager.h"
#include "rbx/rbxTime.h"

#include "util/Http.h"
#include "util/MemoryStats.h"
#include "util/standardout.h"
#include "v8datamodel/Stats.h"
#include "ErrorUploader.h"

#ifdef _WIN32
#include "WinTypes.h"
#else // Linux includes and definitions
#include <boost/interprocess/exceptions.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <sys/types.h>
#include <unistd.h>

// Define MAX_PATH for Linux, as it's a Windows constant
#define MAX_PATH 260
// Define generic HANDLE type for Linux, assuming it's used as a void*
typedef void *HANDLE;
#endif

namespace io = boost::iostreams;

boost::scoped_ptr<RBX::Http> DumpErrorUploader::crashEventRequest;
std::istringstream DumpErrorUploader::crashEventData("Crash happened!");
std::string DumpErrorUploader::crashEventResponse;
std::string DumpErrorUploader::crashCounterNamePrefix;

DYNAMIC_FASTINTVARIABLE(RCCInfluxHundredthsPercentage, 1000)
DYNAMIC_FASTFLAGVARIABLE(ExtendedCrashInfluxReporting, false)

DumpErrorUploader::DumpErrorUploader(
    bool backgroundUpload, const std::string &crashCounterNamePrefix) {
  if (backgroundUpload)
    thread.reset(new RBX::worker_thread(
        boost::bind(&DumpErrorUploader::run, _data), "ErrorUploader"));

  this->crashCounterNamePrefix = crashCounterNamePrefix;
}

void DumpErrorUploader::InitCrashEvent(const std::string &url,
                                       const std::string &crashEventFileName) {
  // Setup a HTTP object for crashEvent
  if (!crashEventRequest) {
    std::string finalUrl =
        url + "?filename=" + RBX::Http::urlEncode(crashEventFileName);
    RBX::Log::current()->writeEntry(
        RBX::Log::Information,
        RBX::format("Initializing CrashEvent request, url: %s",
                    finalUrl.c_str())
            .c_str());
    crashEventRequest.reset(new RBX::Http(finalUrl));
    crashEventResponse.reserve(MAX_PATH);
  }
}

void DumpErrorUploader::Upload(const std::string &url) {
  HANDLE hMutex = NULL;

#ifdef _WIN32
  hMutex = CreateMutex(NULL, // default security descriptor
                       TRUE, // own the mutex
                       TEXT("RobloxCrashDumpUploaderMutex")); // object name

  if (hMutex == NULL) {
    RBX::StandardOut::singleton()->printf(
        RBX::MESSAGE_ERROR, "CreateMutex error: %d\n", GetLastError());
  } else {
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
      RBX::StandardOut::singleton()->printf(
          RBX::MESSAGE_INFO,
          "RobloxCrashDumpUploaderMutex already exists. Not uploading logs.");
      return;
    }
  }
#else // Linux
  boost::interprocess::named_mutex *linuxMutex = nullptr;
  try {
    // Attempt to create the mutex. If it already exists, this will throw.
    linuxMutex = new boost::interprocess::named_mutex(
        boost::interprocess::create_only, "RobloxCrashDumpUploaderMutex");
    // If created successfully, lock it immediately (similar to CreateMutex(...,
    // TRUE, ...))
    linuxMutex->lock();
    RBX::StandardOut::singleton()->printf(
        RBX::MESSAGE_INFO, "RobloxCrashDumpUploaderMutex created and locked.");
    hMutex = static_cast<HANDLE>(linuxMutex);
  } catch (const boost::interprocess::interprocess_exception &ex) {
    if (ex.get_error_code() == boost::interprocess::already_exists_error) {
      RBX::StandardOut::singleton()->printf(
          RBX::MESSAGE_INFO,
          "RobloxCrashDumpUploaderMutex already exists. Not uploading logs.");
      return;
    } else {
      RBX::StandardOut::singleton()->printf(
          RBX::MESSAGE_ERROR, "Error creating named mutex: %s", ex.what());
      return; // Treat other interprocess errors as fatal for upload
    }
  } catch (const std::exception &e) {
    RBX::StandardOut::singleton()->printf(
        RBX::MESSAGE_ERROR, "Unhandled exception creating named mutex: %s",
        e.what());
    return; // Treat other exceptions as fatal for upload
  }
#endif

  std::vector<std::string> f =
      MainLogManager::getMainLogManager()->gatherCrashLogs();

  {
    boost::recursive_mutex::scoped_lock lock(_data->sync);
    _data->url = url;
    _data->hInterprocessMutex = hMutex;
    for (size_t i = 0; i < f.size(); ++i)
      _data->files.push(f[i]);
  }

  if (thread) {
    thread->wake();
  } else {
    while (run(_data) != RBX::worker_thread::done) {
    }
  }
}

#ifdef _WIN32
int LogFilter(unsigned int code, struct _EXCEPTION_POINTERS *ep) {
  RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR,
                                        "Exception code: %u", code);

  return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void DumpErrorUploader::UploadCrashEventFile(
#ifdef _WIN32
    struct _EXCEPTION_POINTERS *excInfo
#else
    void *excInfo // Use a generic pointer for Linux crash info (e.g.,
                  // ucontext_t*, siginfo_t*)
#endif
) {
  try {
    if (crashEventRequest) {
      RBX::StandardOut::singleton()->printf(RBX::MESSAGE_INFO,
                                            "Crash Event post");
      crashEventRequest->post(crashEventData,
                              RBX::Http::kContentTypeDefaultUnspecified, true,
                              crashEventResponse);
      RBX::StandardOut::singleton()->printf(RBX::MESSAGE_INFO,
                                            "Crash Event posted, response: %s",
                                            crashEventResponse.c_str());

      RBX::Analytics::EphemeralCounter::reportCounter(
          crashCounterNamePrefix + "CrashEvent", 1, true);

      RBX::Analytics::InfluxDb::Points points;
      if (DFFlag::ExtendedCrashInfluxReporting) {
        points.addPoint("SessionReport", "AppStatusCrash");
        points.addPoint("PlayTime", RBX::Time::nowFastSec());
        points.addPoint("UsedMemoryKB", RBX::MemoryStats::usedMemoryBytes());

        MainLogManager::GameState gamestate =
            MainLogManager::getMainLogManager()->getGameState();
        std::string gameStateString =
            gamestate == MainLogManager::UN_INITIALIZED
                ? "uninitialized"
                : (gamestate == MainLogManager::IN_GAME ? "inGame"
                                                        : "leaveGame");
        points.addPoint("GameState", gameStateString.c_str());

#ifdef _WIN32
        if (excInfo) {
          points.addPoint("ExceptionCode",
                          (uint32_t)excInfo->ExceptionRecord->ExceptionCode);
          std::stringstream excepAddress;
          excepAddress << excInfo->ExceptionRecord->ExceptionAddress;
          points.addPoint("ExceptionAddress", excepAddress.str().c_str());

          for (uint32_t i = 0; i < excInfo->ExceptionRecord->NumberParameters;
               ++i) {
            std::ostringstream paramName;
            paramName << "ExceptionParameter-" << i;
            std::stringstream paramValue;
            paramValue << excInfo->ExceptionRecord->ExceptionInformation[i];
            points.addPoint(paramName.str(), paramValue.str().c_str());
          }
        }
#else // Linux
        if (excInfo) {
          // For Linux, specific crash details would typically come from parsing
          // a ucontext_t or siginfo_t passed from a signal handler. Without
          // specific parsing logic, we can add generic Linux-specific crash
          // info if excInfo is not null.
          points.addPoint("ExceptionPlatform", "Linux");
          // Add more specific Linux crash details here if `excInfo` can be
          // parsed (e.g., from `ucontext_t`) Example (requires casting and
          // knowledge of ucontext_t structure): ucontext_t* ctx =
          // static_cast<ucontext_t*>(excInfo); if (ctx) {
          // points.addPoint("ProgramCounter",
          // reinterpret_cast<uint64_t>(ctx->uc_mcontext.gregs[REG_RIP])); }
        }
#endif
      }
      points.addPoint(
          "SessionID",
          MainLogManager::getMainLogManager()->getSessionId().c_str());
      points.report(crashCounterNamePrefix + "CrashEvent",
                    DFInt::RCCInfluxHundredthsPercentage);
    }
  } catch (const std::exception &e) {
    RBX::StandardOut::singleton()->printf(
        RBX::MESSAGE_ERROR, "Exception during upload crash event: %s",
        e.what());
  } catch (...) {
    RBX::StandardOut::singleton()->printf(
        RBX::MESSAGE_ERROR, "Exception during upload crash event");
  }
}

RBX::worker_thread::work_result DumpErrorUploader::run(shared_ptr<data> _data) {
  std::string file;
  {
    boost::recursive_mutex::scoped_lock lock(_data->sync);
    if (_data->files.empty()) {
      if (_data->hInterprocessMutex) {
#ifdef _WIN32
        CloseHandle(_data->hInterprocessMutex);
#else // Linux
        boost::interprocess::named_mutex *mutex =
            static_cast<boost::interprocess::named_mutex *>(
                _data->hInterprocessMutex);
        if (mutex) {
          try {
            mutex->unlock(); // Ensure mutex is unlocked before destruction
          } catch (const boost::interprocess::interprocess_exception &e) {
            RBX::StandardOut::singleton()->printf(
                RBX::MESSAGE_WARNING, "Error unlocking named mutex: %s",
                e.what());
          }
          delete mutex;
        }
#endif
        _data->hInterprocessMutex = NULL;
      }

      if (_data->dmpFileCount > 0) {
        // report number of dmps uploaded
        RBX::Analytics::EphemeralCounter::reportCounter(
            crashCounterNamePrefix + "Crash", _data->dmpFileCount, true);
      }

      return RBX::worker_thread::done;
    }
    file = _data->files.front();
  }

  try {
    printf("Uploading %s\n", file.c_str());
    // .dmp files are Windows crash dumps, but we keep the check in case Linux
    // tools also output them or similar logic is needed.
    bool isDmpFile = file.substr(file.size() - 4) == ".dmp";
    if (isDmpFile)
      _data->dmpFileCount++;

    bool isFullDmp = file.find(".Full.") != std::string::npos;

    // TODO: put the filename in the header rather than the query string??
    std::string url = _data->url;
    url += "?filename=";
    url += RBX::Http::urlEncode(file);

    if (isDmpFile && _data->dmpFileCount > 3) {
      std::stringstream data("Too many dmp files");
      std::string response;
      RBX::Http(url).post(data, RBX::Http::kContentTypeDefaultUnspecified, true,
                          response);
    } else if (!isFullDmp) {
      RBX::StandardOut::singleton()->printf(RBX::MESSAGE_INFO, "Uploading %s\n",
                                            file.c_str());
      std::fstream data(file.c_str(),
                        std::ios_base::in | std::ios_base::binary);
      std::streamoff begin = data.tellg();
      data.seekg(0, std::ios::end);
      std::streamoff end = data.tellg();
      if (end > begin) {
        data.seekg(0, std::ios::beg);
        std::string response;
        RBX::Http(url).post(data, RBX::Http::kContentTypeDefaultUnspecified,
                            true, response);
      } else {
        // Some dmp files are empty. Post it anyway so that we can report it
        std::stringstream data("Empty!!!");
        std::string response;
        RBX::Http(url).post(data, RBX::Http::kContentTypeDefaultUnspecified,
                            true, response);
      }
    }

    // done uploading, move to archive.
    // nb: if upload cuts right after uploading .dmp file, we will never upload
    // the other log files associated. we think this is acceptable to keep this
    // code simple at this time.
    ErrorUploader::MoveRelative(file,
#ifdef _WIN32
                                "archive\\"
#else
                                "archive/"
#endif
    );
  } catch (std::exception &e) {
    RBX::StandardOut::singleton()->print(RBX::MESSAGE_ERROR, e);
  }

  {
    boost::recursive_mutex::scoped_lock lock(_data->sync);
    _data->files.pop();
  }
  return RBX::worker_thread::more;
}
