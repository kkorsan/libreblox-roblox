/* Copyright 2003-2009 ROBLOX Corporation, All Rights Reserved */

#pragma once

#include <string>
#include <istream>
#include "rbx/Boost.hpp"
#include "rbx/rbxTime.h"
#include "v8tree/Service.h"
#include "util/AsyncHttpCache.h"
#define BOOST_DATE_TIME_NO_LIB
#include "boost/date_time/posix_time/posix_time.hpp"
#include "util/HeartbeatInstance.h"

namespace RBX
{
	class Instance;

	extern const char* const sScriptInformationProvider;
	class ScriptInformationProvider
		: public DescribedNonCreatable<ScriptInformationProvider, Instance, sScriptInformationProvider>
		, public Service
	{
	private:
		typedef DescribedNonCreatable<ScriptInformationProvider, Instance, sScriptInformationProvider> Super;

		std::string assetUrl;
		std::string access;

	public:
		ScriptInformationProvider();

		void setAssetUrl(std::string url)
		{
			assetUrl = url;
		}
		void setAccessKey(std::string access)
		{
			this->access = access;
		}
	};
} //namespace
