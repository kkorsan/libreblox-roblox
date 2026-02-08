#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

#if !defined(_WIN32)
#include <unistd.h>
#include <wchar.h>

// POSIX version of _vscprintf
inline int _vscprintf(const char* format, va_list argptr)
{
    return vsnprintf(NULL, 0, format, argptr);
}

inline int _vscwprintf(const wchar_t* format, va_list argptr)
{
    return vswprintf(NULL, 0, format, argptr);
}

static const size_t _TRUNCATE = static_cast<size_t>(-1);

inline int vsnprintf_s(char *buffer, size_t sizeOfBuffer, size_t count, const char *format, va_list argptr) {
    if (count == _TRUNCATE) {
        return vsnprintf(buffer, sizeOfBuffer, format, argptr);
    } else {
        return vsnprintf(buffer, (count < sizeOfBuffer) ? count : sizeOfBuffer, format, argptr);
    }
}

inline int _vsnwprintf_s(wchar_t *buffer, size_t sizeOfBuffer, size_t count, const wchar_t *format, va_list argptr) {
    if (count == _TRUNCATE) {
        return vswprintf(buffer, sizeOfBuffer, format, argptr);
    } else {
        return vswprintf(buffer, (count < sizeOfBuffer) ? count : sizeOfBuffer, format, argptr);
    }
}
#endif

std::string convert_w2s(const std::wstring &str)
{
#ifdef _WIN32
    if (str.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size_needed, NULL, NULL);
    return result;
#else
    std::string result;
    result.reserve(str.length());
    for (wchar_t wc : str) {
        if (wc < 0x80) {
            result += static_cast<char>(wc);
        } else {
            // Simple UTF-8 encoding for basic cases
            // You might want a more complete implementation
            result += '?'; // fallback
        }
    }
    return result;
#endif
}

std::wstring convert_s2w(const std::string &str)
{
#ifdef _WIN32
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size_needed);
    return result;
#else
    std::wstring result;
    result.reserve(str.length());
    for (unsigned char c : str) {
        result += static_cast<wchar_t>(c);
    }
    return result;
#endif
}

std::string vformat(const char *fmt, va_list argPtr) {
    // We draw the line at a 1MB string.
    const int maxSize = 1000000;

    // If the string is less than 161 characters,
    // allocate it on the stack because this saves
    // the malloc/free time.
    const int bufSize = 161;
    char stackBuffer[bufSize];

    // Make a copy of argPtr since we'll use it twice
    va_list argPtrCopy;
    va_copy(argPtrCopy, argPtr);
    int actualSize = _vscprintf(fmt, argPtrCopy);
    va_end(argPtrCopy);

    if (actualSize < 0) {
        throw std::runtime_error("Format string error");
    }

    actualSize++; // Add space for null terminator

    if (actualSize > bufSize) {
        // Now use the heap.
        if (actualSize > maxSize) {
            throw std::runtime_error("Format string too large");
        }

        char* heapBuffer = (char*)malloc(actualSize);
        if (!heapBuffer) {
            throw std::bad_alloc();
        }

        int result = vsnprintf_s(heapBuffer, actualSize, _TRUNCATE, fmt, argPtr);
        if (result < 0) {
            free(heapBuffer);
            throw std::runtime_error("Format string error");
        }

        std::string formattedString(heapBuffer);
        free(heapBuffer);
        return formattedString;
    } else {
        int result = vsnprintf_s(stackBuffer, bufSize, _TRUNCATE, fmt, argPtr);
        if (result < 0) {
            throw std::runtime_error("Format string error");
        }
        return std::string(stackBuffer);
    }
}

std::string format_string(const char* fmt, ...) {
    va_list argList;
    va_start(argList, fmt);
    std::string result = vformat(fmt, argList);
    va_end(argList);
    return result;
}

#ifdef _WIN32
std::wstring vformat(const wchar_t *fmt, va_list argPtr) {
    // We draw the line at a 1MB string.
    const int maxSize = 1000000;

    // If the string is less than 161 characters,
    // allocate it on the stack because this saves
    // the malloc/free time.
    const int bufSize = 161;
    wchar_t stackBuffer[bufSize];

    // Make a copy of argPtr since we'll use it twice
    va_list argPtrCopy;
    va_copy(argPtrCopy, argPtr);
    int actualSize = _vscwprintf(fmt, argPtrCopy);
    va_end(argPtrCopy);

    if (actualSize < 0) {
        throw std::runtime_error("Format string error");
    }

    actualSize++; // Add space for null terminator

    if (actualSize > bufSize) {
        // Now use the heap.
        if (actualSize > maxSize) {
            throw std::runtime_error("Format string too large");
        }

        wchar_t* heapBuffer = (wchar_t*)malloc(actualSize * sizeof(wchar_t));
        if (!heapBuffer) {
            throw std::bad_alloc();
        }

        int result = _vsnwprintf_s(heapBuffer, actualSize, _TRUNCATE, fmt, argPtr);
        if (result < 0) {
            free(heapBuffer);
            throw std::runtime_error("Format string error");
        }

        std::wstring formattedString(heapBuffer);
        free(heapBuffer);
        return formattedString;
    } else {
        int result = _vsnwprintf_s(stackBuffer, bufSize, _TRUNCATE, fmt, argPtr);
        if (result < 0) {
            throw std::runtime_error("Format string error");
        }
        return std::wstring(stackBuffer);
    }
}

std::wstring format_string(const wchar_t* fmt, ...) {
    va_list argList;
    va_start(argList, fmt);
    std::wstring result = vformat(fmt, argList);
    va_end(argList);
    return result;
}
#endif

// Split functions look fine, but here's a slightly cleaned up version:
std::vector<std::string> splitOn(const std::string& str, const char& delimiter, const bool trimEmpty = true) {
    std::vector<std::string> tokens;
    std::size_t begin = 0;
    std::size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        std::string tmp = str.substr(begin, end - begin);
        if (!tmp.empty() || !trimEmpty)
            tokens.push_back(tmp);

        begin = end + 1;
        end = str.find(delimiter, begin);
    }

    // Handle the last token
    std::string tmp = str.substr(begin);
    if (!tmp.empty() || !trimEmpty)
        tokens.push_back(tmp);

    return tokens;
}

std::vector<std::wstring> splitOn(const std::wstring& str, const wchar_t& delimiter, const bool trimEmpty = true) {
    std::vector<std::wstring> tokens;
    std::size_t begin = 0;
    std::size_t end = str.find(delimiter);

    while (end != std::wstring::npos) {
        std::wstring tmp = str.substr(begin, end - begin);
        if (!tmp.empty() || !trimEmpty)
            tokens.push_back(tmp);

        begin = end + 1;
        end = str.find(delimiter, begin);
    }

    // Handle the last token
    std::wstring tmp = str.substr(begin);
    if (!tmp.empty() || !trimEmpty)
        tokens.push_back(tmp);

    return tokens;
}
