#include "stdafx.h"
#include "VersionInfo.h"

CVersionInfo::CVersionInfo()
{
}

CVersionInfo::~CVersionInfo()
{
}

void CVersionInfo::Load()
{
    // Implementation for Load method
    // This method is called to load version information
    // For now, we'll provide a basic implementation
}

std::string CVersionInfo::GetFileVersionAsString()
{
    return "1.0.0.0";
}

std::string CVersionInfo::GetFileVersionAsDotString()
{
    return "1.0.0.0";
}
