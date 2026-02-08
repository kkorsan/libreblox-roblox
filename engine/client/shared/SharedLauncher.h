/**
 * SharedLauncher.h
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#pragma once

#include <string>
#include <vector>

// Define Windows types if on Windows, otherwise stub them for other platforms
#if defined(_WIN32)
    #include <windows.h>
    #include <tchar.h>
    #include <oleauto.h> // For BSTR, HRESULT, VARIANT_BOOL
    typedef HKEY RBX_REG_KEY;
#else
    #include <stdint.h>
#endif

#include "format_string.h"
#include "ProcessInformation.h"

#ifndef LAUNCHER_STARTED_EVENT_NAME
    #define LAUNCHER_STARTED_EVENT_NAME _T("rbxLauncherStarted")
#endif

namespace SharedLauncher
{
    static const char* FileLocationArgument = "-fileLocation";
    static const char* ScriptArgument       = "-script";
    static const char* AuthUrlArgument      = "-url";
    static const char* AuthTicketArgument   = "-ticket";
    static const char* StartEventArgument   = "-startEvent";
    static const char* ReadyEventArgument   = "-readyEvent";
    static const char* ShowEventArgument    = "-showEvent";
    static const char* TestModeArgument     = "-testMode";
    static const char* IDEArgument          = "-ide";
    static const char* BuildArgument        = "-build";
    static const char* DebuggerArgument     = "-debugger";
    static const char* AvatarModeArgument   = "-avatar";
    static const char* RbxDevArgument       = "-rbxdev";
    static const char* BrowserTrackerId     = "-browserTrackerId";

    enum LaunchMode
    {
        Play,
        Play_Protocol,
        Build,
        Edit
    };

    struct EditArgs
    {
        std::string fileName;
        std::string authUrl;
        std::string authTicket;
        std::string script;
        std::string readyEvent;
        std::string showEventName;
        std::string launchMode;
        std::string avatarMode;
        std::string browserTrackerId;
    };

    std::wstring generateEditCommandLine(const EditArgs& editArgs);
    bool parseEditCommandArg(wchar_t** args, int& index, int count, EditArgs& editArgs);
}
