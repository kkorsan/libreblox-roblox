#pragma once

#ifdef __linux__

#include <cstdint>

#ifndef DWORD
typedef unsigned long DWORD;
#endif

#ifndef Sleep
#define Sleep(ms) usleep(ms * 1000)
#endif

typedef long LONG;
typedef unsigned short WORD;
typedef const char* LPCTSTR;
typedef const char* LPCSTR;
typedef const wchar_t* LPCOLESTR;
typedef void* HANDLE;
typedef void* HMODULE;
typedef long HRESULT;

#define MAX_PATH 260

#define EVENTLOG_SUCCESS                0x0000
#define EVENTLOG_ERROR_TYPE             0x0001
#define EVENTLOG_WARNING_TYPE           0x0002
#define EVENTLOG_INFORMATION_TYPE       0x0004
#define EVENTLOG_AUDIT_SUCCESS          0x0008
#define EVENTLOG_AUDIT_FAILURE          0x0010

#endif
