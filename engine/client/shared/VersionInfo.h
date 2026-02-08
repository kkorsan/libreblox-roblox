#pragma once

#include "WinTypes.h"
#include <string>

class CVersionInfo
{
public:
    CVersionInfo();
    ~CVersionInfo();

    void Load();
    std::string GetFileVersionAsString();
    std::string GetFileVersionAsDotString();
};