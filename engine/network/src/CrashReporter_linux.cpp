/*
 * CrashReporter_linux.cpp - Linux implementation of CrashReporter
 *
 * This stub provides a minimal Linux implementation that meets the
 * interface defined in CrashReporter.h. It logs events via RBX::Log,
 * spawns a watcher thread, and provides dummy implementations for
 * crash processing and minidump filename generation.
 *
 * You can expand this implementation to perform actual crash reporting,
 * core-dump processing, or file uploading as needed.
 */

#include "CrashReporter.h"
#include "rbx/Log.h"
#include "rbx/TaskScheduler.h"
#include <boost/thread.hpp>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <cstdio>
#include <cstring>

// For HRESULT codes on Linux, you might define S_OK and E_FAIL:
#ifndef S_OK
#define S_OK 0
#endif
#ifndef E_FAIL
#define E_FAIL 0x80004005
#endif
CrashReporter* CrashReporter::singleton = nullptr;


CrashReporter::CrashReporter() :
    threadResult(0),
    exceptionInfo(nullptr),
    reportCrashEvent(false),  // Default constructor
    watcherThread(),
    hangReportingEnabled(false),
    isAlive(true),
    deadlockCounter(0),
    destructing(false),
    immediateUploadEnabled(true),
    silentCrashReporting(false)
{
    singleton = this;
    if (RBX::Log::current())
        RBX::Log::current()->writeEntry(RBX::Log::Information, "CrashReporter Linux stub created.");
}

CrashReporter::~CrashReporter() {
    destructing = true;
    if (watcherThread) {
        watcherThread->join();
        watcherThread.reset();
    }
    if (RBX::Log::current())
        RBX::Log::current()->writeEntry(RBX::Log::Information, "CrashReporter Linux stub destroyed.");
}

void CrashReporter::Start() {
    // Launch the watcher thread to "monitor" for hangs/crashes.
    watcherThread.reset(new boost::thread(boost::bind(&CrashReporter::WatcherThreadFunc, this)));
}

void CrashReporter::WatcherThreadFunc() {
    if (RBX::Log::current())
        RBX::Log::current()->writeEntry(RBX::Log::Information, "CrashReporter Linux WatcherThread started (stub).");
    // In this stub, we simply sleep for a fixed period (simulate monitoring) and then exit.
    // In a more complete implementation, you might poll for responsiveness, etc.
    while (!destructing) {
        sleep(5);  // wait 5 seconds
        // For stub purposes, call NotifyAlive() to indicate activity.
        NotifyAlive();
        break;  // exit after one iteration
    }
    threadResult = 0;
    if (RBX::Log::current())
        RBX::Log::current()->writeEntry(RBX::Log::Information, "CrashReporter Linux WatcherThread exiting (stub).");
}

LONG CrashReporter::ProcessException(struct _EXCEPTION_POINTERS* /*ExceptionInfo*/, bool /*noMsg*/) {
    // Linux stub: simply log that an exception was "processed" and return success.
    if (RBX::Log::current())
        RBX::Log::current()->writeEntry(RBX::Log::Error, "CrashReporter Linux stub: ProcessException called.");
    return 0;
}

LONG CrashReporter::ProcessExceptionInThead(struct _EXCEPTION_POINTERS* ExceptionInfo) {
    return ProcessException(ExceptionInfo, false);
}

void CrashReporter::TheadFunc(struct _EXCEPTION_POINTERS* ExceptionInfo) {
    threadResult = ProcessException(ExceptionInfo, false);
}

void CrashReporter::DisableHangReporting() {
    hangReportingEnabled = false;
}

void CrashReporter::EnableImmediateUpload(bool enabled) {
    immediateUploadEnabled = enabled;
}

void CrashReporter::NotifyAlive() {
    if (RBX::Log::current())
        RBX::Log::current()->writeEntry(RBX::Log::Information, "CrashReporter Linux stub: NotifyAlive called.");
    isAlive = true;
}

HRESULT CrashReporter::GenerateDmpFileName(char* dumpFilepath, int cchdumpFilepath, bool fastLog, bool fullDmp) {
    // For Linux stub, simply generate a dummy filename using the appName and appVersion from controls.
    if (!dumpFilepath || cchdumpFilepath <= 0)
        return E_FAIL;

    // For example: "<appName>_<appVersion>[.Full][.txt or crashExtention]"
    snprintf(dumpFilepath, cchdumpFilepath, "%s_%s%s%s",
             controls.appName,
             controls.appVersion,
             fullDmp ? ".Full" : "",
             fastLog ? ".txt" : controls.crashExtention);
    return S_OK;
}

void CrashReporter::LaunchUploadProcess() {
    if (!immediateUploadEnabled)
        return;
    // Stub: Log that an upload process would be launched.
    if (RBX::Log::current())
        RBX::Log::current()->writeEntry(RBX::Log::Information, "CrashReporter Linux stub: LaunchUploadProcess called.");
    // If needed, you could spawn a separate process here to upload the dump.
}
