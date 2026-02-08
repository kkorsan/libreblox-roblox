#include "stdafx.h"

// HACK! This avoids "nil" macro compatibility issues between MacOS SDK and Boost
#ifdef nil
#undef nil
#endif

#include <algorithm>

#include "g3d/format.h"

#include "util/Http.h"
#include "util/HttpPlatformImpl.h"
#include "util/URL.h"
#include "util/RobloxGoogleAnalytics.h"
#include "util/standardout.h"
#include "util/Statistics.h"
#include "util/ThreadPool.h"
#include "util/Analytics.h"
#include "util/RobloxGoogleAnalytics.h"

#include "rbx/TaskScheduler.h"

#include "v8datamodel/DataStore.h"
#include "v8datamodel/DebugSettings.h"
#include "v8datamodel/MarketplaceService.h"
#include "v8datamodel/Stats.h"
#include "RobloxServicesTools.h"



#if defined(_WIN32) && !defined(RBX_PLATFORM_DURANGO) && defined(_MSC_VER) // msvc only!
#include <atlutil.h>
#endif // defined(_WIN32) && !defined(RBX_PLATFORM_DURANGO)

LOGGROUP(Http)
DYNAMIC_FASTINT(ExternalHttpRequestSizeLimitKB)
DYNAMIC_FASTINTVARIABLE(HttpResponseDefaultTimeoutMillis, 60000)
DYNAMIC_FASTINTVARIABLE(HttpSendDefaultTimeoutMillis, 60000)
DYNAMIC_FASTINTVARIABLE(HttpConnectDefaultTimeoutMillis, 60000)
DYNAMIC_FASTINTVARIABLE(HttpDataSendDefaultTimeoutMillis, 60000)

DYNAMIC_LOGVARIABLE(HttpTrace, 0)
DYNAMIC_FASTINTVARIABLE(HttpSendStatsEveryXSeconds, 60)
DYNAMIC_FASTINTVARIABLE(HttpGAFailureReportPercent, 1)
DYNAMIC_FASTINTVARIABLE(HttpRBXEventFailureReportHundredthsPercent, 0)
DYNAMIC_FASTFLAGVARIABLE(DebugHttpAsyncCallsForStatsReporting, true)
DYNAMIC_FASTINT(HttpInfluxHundredthsPercentage)
DYNAMIC_FASTFLAG(UseNewAnalyticsApi)
DYNAMIC_FASTSTRING(HttpInfluxURL)

DYNAMIC_FASTFLAGVARIABLE(UseAssetTypeHeader, false)

using namespace RBX;

#ifdef __APPLE__
extern "C" {
    int rbx_trustCheckBrowser(const char* url)
    {
        return RBX::Http::trustCheckBrowser(url) ? 1 : 0;
    }
}

// NOTICE: Remove these calls after transition for Apple is complete.
int rbx_isRobloxSite(const char* url);
int rbx_isMoneySite(const char* url);
#endif // ifdef __APPLE__

namespace
{
#if defined(RBX_PLATFORM_DURANGO)
static bool useCurlHttpImpl = false;
#else
static bool useCurlHttpImpl = true;
#endif

static const int kNumberThreadPoolThreads = 16;
static ThreadPool* threadPool;

static bool inline sendHttpFailureToEvents()
{
    static const int r = rand() % 10000;
    return r < DFInt::HttpRBXEventFailureReportHundredthsPercent;
}

static bool inline sendHttpFailureToGA()
{
    static const int r = rand() % 100;
    return r < DFInt::HttpGAFailureReportPercent;
}

static bool inline sendHttpInfluxEvents()
{
    static const int r = rand() % 10000;
    return r < DFInt::HttpInfluxHundredthsPercentage;
}

bool isValidScheme(const char* scheme)
{
    return 0 == strncmp(scheme, "http", 4) || 0 == strncmp(scheme, "https", 5);
}

bool hasEnding(const std::string& fullString, const std::string& ending)
{
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

class HTTPStatistics
{
    static HTTPStatistics httpStatistics;

    boost::mutex mutex;

    double successDelays;
    double failureDelays;
    unsigned numSuccesses;
    unsigned numFailures;

    unsigned numDataStoreSuccesses;
    unsigned numDataStoreFailures;

    unsigned numMarketPlaceSuccesses;
    unsigned numMarketPlaceFailures;

    size_t successBytes;

    enum ServiceCategory
    {
        ServiceCategoryDataStore,
        ServiceCategoryMarketPlace,
        ServiceCategoryOther
    };

	struct PendingStat
	{
		std::string url;
		std::string reason;
		double delay;
		size_t size;
		int statusCode;

		PendingStat(const char* url, const char* reason, size_t bytes, double delay, int httpStatusCode)
			: url(url)
			, reason(reason)
			, delay(delay)
			, size(bytes)
			, statusCode(httpStatusCode)
		{}
	};

	std::list<PendingStat> pendingStats;

    HTTPStatistics()
        :successDelays(0.0)
        ,failureDelays(0.0)
        ,numSuccesses(0)
        ,numFailures(0)
        ,numDataStoreSuccesses(0)
        ,numDataStoreFailures(0)
        ,numMarketPlaceSuccesses(0)
        ,numMarketPlaceFailures(0)
        ,successBytes(0)
    {}

    void addSuccess(double delay, const char* url, size_t bytes)
    {
        const boost::mutex::scoped_lock lock(mutex);
        ++numSuccesses;
        successDelays += delay;
        successBytes += bytes;

        switch (getServiceCategory(url))
        {
        case ServiceCategoryDataStore:
            ++numDataStoreSuccesses;
            break;
        case ServiceCategoryMarketPlace:
            ++numMarketPlaceSuccesses;
            break;
        default:
            break;
        }
    }

    void addFailure(double delay, const char* url, const char* reason)
    {
        const boost::mutex::scoped_lock lock(mutex);
        ++numFailures;
        failureDelays += delay;

        switch (getServiceCategory(url))
        {
        case ServiceCategoryDataStore:
            ++numDataStoreFailures;
            break;
        case ServiceCategoryMarketPlace:
            ++numMarketPlaceFailures;
            break;
        default:
            break;
        }

        // Report immediately to GA, but we are assuming failure rates are
        // low so that we don't end up with an infinite recursion when
        // reporting failure data.
        if (sendHttpFailureToGA())
        {
            RobloxGoogleAnalytics::trackEvent(GA_CATEGORY_GAME, "HTTPFailure", reason, delay);
        }

        if (sendHttpFailureToEvents())
        {
            int placeID = atoi(Http::placeID.c_str());
            RobloxGoogleAnalytics::sendEventRoblox(GA_CATEGORY_GAME, "HTTPFailure", reason, placeID);
        }
    }

	void addPendingStat(double delay, const char* url, size_t bytes, const char* reason, int httpStatusCode = 0)
	{
		const boost::mutex::scoped_lock lock(mutex);
		pendingStats.push_back(PendingStat(url, reason, bytes, delay, httpStatusCode));
	}

    void report()
    {
        const boost::mutex::scoped_lock lock(mutex);

		// No need for this to be blocking any more as per CS 61109 on CI
		// If we keep this blocking & suppose the influx goes down or is taking too much time
		// Then we will have to wait 60 sec for each of this request to time out and we will have too many jobs waiting for the mutex to be released
		// Keeping it behind a Flag if we need to flip it other way. Do not remove DebugHttpAsyncCallsForStatsReporting
        if (numSuccesses)
        {
            Analytics::EphemeralCounter::reportStats("HTTPDelaySuccess-"+DebugSettings::singleton().osPlatform(), successDelays / numSuccesses, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
            Analytics::EphemeralCounter::reportCounter("HTTPSuccess-"+DebugSettings::singleton().osPlatform(), numSuccesses, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
            Analytics::EphemeralCounter::reportCounter("HTTPSuccessDataStore-"+DebugSettings::singleton().osPlatform(), numDataStoreSuccesses, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
            Analytics::EphemeralCounter::reportCounter("HTTPSuccessMarketPlace-"+DebugSettings::singleton().osPlatform(), numMarketPlaceSuccesses, !DFFlag::DebugHttpAsyncCallsForStatsReporting);

            Analytics::EphemeralCounter::reportStats("HTTPAccumulatedBytes-"+DebugSettings::singleton().osPlatform(), successBytes, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
        }

        if (numFailures)
        {
            Analytics::EphemeralCounter::reportStats("HTTPDelayFailure-"+DebugSettings::singleton().osPlatform(), failureDelays / numFailures, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
            Analytics::EphemeralCounter::reportCounter("HTTPFailure-"+DebugSettings::singleton().osPlatform(), numFailures, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
            Analytics::EphemeralCounter::reportCounter("HTTPFailureDataStore-"+DebugSettings::singleton().osPlatform(), numDataStoreFailures, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
            Analytics::EphemeralCounter::reportCounter("HTTPFailureMarketPlace-"+DebugSettings::singleton().osPlatform(), numMarketPlaceFailures, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
        }

        successDelays = failureDelays = 0.0;
        numSuccesses = numFailures = 0;
        numDataStoreSuccesses = numDataStoreFailures = 0;
        numMarketPlaceSuccesses = numMarketPlaceFailures = 0;
        successBytes = 0;

		if (!pendingStats.empty())
		{
			for (std::list<PendingStat>::iterator i = pendingStats.begin(); i != pendingStats.end(); i++)
			{
				sendInfluxEvent(i->delay, i->url.c_str(), i->size, i->reason.c_str(), i->statusCode, !DFFlag::DebugHttpAsyncCallsForStatsReporting);
			}
		}

		pendingStats.clear();
    }

    static ServiceCategory getServiceCategory(const char* url)
    {
        RBX::Url parsed = RBX::Url::fromString(url);
        if (parsed.pathEquals(DataStore::urlApiPath()))
        {
            return ServiceCategoryDataStore;
        }

        if (parsed.pathEquals(MarketplaceService::urlApiPath()))
        {
            return ServiceCategoryMarketPlace;
        }
        return ServiceCategoryOther;
    }

    static const char* serviceToString(const char* url)
    {
        switch (getServiceCategory(url))
        {
        case ServiceCategoryDataStore:
            return "DataStore";
        case ServiceCategoryMarketPlace:
            return "MarketPlace";
        default:
            return "Unknown";
        }
    }

    static void sendInfluxEvent(double delay, const char* url, size_t bytes, const char* reason, int httpStatusCode = 0, bool blocking = false)
    {
        if (!DFFlag::UseNewAnalyticsApi && !sendHttpInfluxEvents())
        {
            return;
        }

		// throttle chat filter requests
		std::string s(url);
		if (s.find("/moderation/filtertext") != std::string::npos)
		{
			const int r = rand() % 10000;
			if (r > DFInt::HttpInfluxHundredthsPercentage)
				return;
		}

        RBX::Analytics::InfluxDb::Points points;
        points.addPoint("DelayMillis", delay);
        points.addPoint("IsSuccess", true);
        points.addPoint("ServiceType", serviceToString(url));
        points.addPoint("Bytes", static_cast<unsigned>(bytes));
        points.addPoint("FailureReason", reason);
        points.addPoint("URL", url);
        points.addPoint("HttpStatusCode", httpStatusCode);

		points.report("http", DFInt::HttpInfluxHundredthsPercentage, blocking);
    }

public:
    static void reportingThreadHandler()
    {
        while (true)
        {
            boost::this_thread::sleep(boost::posix_time::seconds(DFInt::HttpSendStatsEveryXSeconds));
            httpStatistics.report();
        }
    }

    static void success(double delay, const char* url, size_t bytes)
    {
        httpStatistics.addSuccess(delay, url, bytes);
		httpStatistics.addPendingStat(delay, url, bytes, "No error");
    }

    static void failure(double delay, const char* url, size_t bytes, const char* reason, int httpStatusCode=0)
    {
		httpStatistics.addFailure(delay, url, reason);
		httpStatistics.addPendingStat(delay, url, bytes, reason, httpStatusCode);
    }
}; // struct CacheStatistics
HTTPStatistics HTTPStatistics::httpStatistics;
} // namespace

namespace RBX
{
static const int kWindowSize = 256;
std::string Http::accessKey;
std::string Http::gameSessionID;
std::string Http::gameID;
std::string Http::placeID;
std::string Http::requester = "Client";
#if defined (__APPLE__) && !defined(RBX_PLATFORM_IOS)
std::string Http::rbxUserAgent = "Roblox/Darwin";
#elif defined(RBX_PLATFORM_DURANGO)
std::string Http::rbxUserAgent = "Roblox/XboxOne";
#else
std::string Http::rbxUserAgent = "Roblox/Curl";
#endif

int Http::playerCount = 0;
bool Http::useDefaultTimeouts = true;
Http::CookieSharingPolicy Http::cookieSharingPolicy;

Http::API Http::defaultApi = Http::Curl;

rbx::atomic<int> Http::cdnSuccessCount = 0;
rbx::atomic<int> Http::cdnFailureCount = 0;
rbx::atomic<int> Http::alternateCdnSuccessCount = 0;
rbx::atomic<int> Http::alternateCdnFailureCount = 0;
double Http::lastCdnFailureTimeSpan = 0;
rbx::atomic<int> Http::robloxSuccessCount = 0;
rbx::atomic<int> Http::robloxFailureCount = 0;
WindowAverage<double, double> Http::robloxResponce(kWindowSize);
WindowAverage<double, double> Http::cdnResponce(kWindowSize);

RBX::mutex *Http::robloxResponceLock = NULL;
RBX::mutex *Http::cdnResponceLock = NULL;

Http::MutexGuard Http::lockGuard;

} // RBX

Http::MutexGuard::MutexGuard()
{
    robloxResponceLock = new RBX::mutex();
    cdnResponceLock = new RBX::mutex();
}

Http::MutexGuard::~MutexGuard()
{
    delete robloxResponceLock;
    robloxResponceLock = NULL;

    delete cdnResponceLock;
    cdnResponceLock = NULL;
}

RBX::mutex *Http::getRobloxResponceLock()
{
    return robloxResponceLock;
}

RBX::mutex *Http::getCdnResponceLock()
{
    return cdnResponceLock;
}

http_status_error::http_status_error(int statusCode)
    : std::runtime_error(RBX::format("HTTP %d", statusCode))
    , statusCode(statusCode)
{
}

http_status_error::http_status_error(int statusCode, const std::string& message)
    : std::runtime_error(RBX::format("HTTP %d (%s)", statusCode, message.c_str()))
    , statusCode(statusCode)
{
}

void Http::init()
{
    recordStatistics = true;
	shouldRetry = true;
    cachePolicy = HttpCache::PolicyDefault;
    doNotUseCachedResponse = false;
    connectTimeoutMillis = DFInt::HttpConnectDefaultTimeoutMillis;
    responseTimeoutMillis = DFInt::HttpResponseDefaultTimeoutMillis;
    sendTimeoutMillis = DFInt::HttpSendDefaultTimeoutMillis;
    dataSendTimeoutMillis = DFInt::HttpDataSendDefaultTimeoutMillis;
    sendHttpFailureToGA(); //initializes the static random number in function
    sendHttpFailureToEvents(); //initializes the static random number in function
    sendHttpInfluxEvents(); //initializes the static random number in function
}

void Http::init(API api, CookieSharingPolicy sharingPolicy)
{
    static boost::scoped_ptr<ThreadPool> tp(new ThreadPool(kNumberThreadPoolThreads));
    threadPool = tp.get();
    Http::defaultApi = api;
    FASTLOG2(FLog::Http, "Http initialization M1 = 0x%x M2=0x%x", &Http::robloxResponceLock, &Http::cdnResponceLock);
    cookieSharingPolicy = sharingPolicy;
	Http::SetUseCurl(useCurlHttpImpl);
}

void Http::SetUseStatistics(bool value)
{
    if (value)
    {
        boost::function0<void> f = boost::bind(&HTTPStatistics::reportingThreadHandler);
        boost::function0<void> g = boost::bind(&StandardOut::print_exception, f, MESSAGE_ERROR, false);
        boost::thread(thread_wrapper(g, "rbx_http_stats_report"));
    }
}

void Http::SetUseCurl(bool value)
{
	FASTLOGS(DFLog::HttpTrace, "Use the new http api: %s", value ? "yes" : "no");
    useCurlHttpImpl = value;

    if (useCurlHttpImpl)
    {
        static boost::mutex mutex;
        static bool initialized = false;

        boost::mutex::scoped_lock lock(mutex);

        if (!initialized)
        {
            HttpPlatformImpl::init(cookieSharingPolicy);
            initialized = true;
        }
    }
}

void Http::setCookiesForDomain(const std::string& domain, const std::string& cookies)
{
    HttpPlatformImpl::setCookiesForDomain(domain, cookies);
}

void Http::getCookiesForDomain(const std::string& domain, std::string& cookies){
  HttpPlatformImpl::getCookiesForDomain(domain, cookies);
}

void Http::ThrowIfFailure(bool success, const char* message)
{
    ThrowIfFailure(success, url.c_str(), message);
}

void Http::ThrowIfFailure(bool success, const char* url, const char* message)
{
    if (!success)
    {
#if defined(_WIN32) && !defined(RBX_PLATFORM_DURANGO)
        ThrowLastError(GetLastError(), url, message);
#else
        throw RBX::runtime_error("%s: %s", url, message);
#endif
    }
}

#if defined(_WIN32) && !defined(RBX_PLATFORM_DURANGO)
void Http::ThrowLastError(int err, const char* url, const char* message)
{
    TCHAR buffer[256];
    if (::FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT),
            buffer, 256, NULL) == 0
        )
        throw RBX::runtime_error("%s: %s, err=0x%X", url, message, err);
    else
        throw RBX::runtime_error("%s: %s, %s", url, message, buffer);
}
#endif // ifdef _WIN32


#if defined(RBX_PLATFORM_DURANGO)
void dprintf( const char* fmt, ... );
#endif

void Http::httpGetPost(bool isPost, std::istream& dataStream,
					   const std::string& contentType, bool compressData,
					   const HttpAux::AdditionalHeaders& additionalHeaders,
					   bool externalRequest, std::string& response,
					   bool forceNativeHttp)
{
    RBX::Timer<RBX::Time::Fast> httpTimer;

    RBXASSERT(isPost == !contentType.empty());

	ThrowIfFailure(trustCheck(url.c_str(), externalRequest), "Trust check failed");

    if (externalRequest)
    {
        if (isPost)
        {
            RBXASSERT(0 == dataStream.tellg());
            dataStream.seekg(0, std::ios::end);
            size_t length = dataStream.tellg();
            dataStream.seekg(0, std::ios::beg);
            if ((length / 1024) >= static_cast<size_t>(DFInt::ExternalHttpRequestSizeLimitKB))
            {
                throw RBX::runtime_error("Post data too large. Limit: %d KB. Post size: %d KB",
                                         DFInt::ExternalHttpRequestSizeLimitKB, static_cast<int>(length / 1024));
            }
        }
    }

    HttpAux::AdditionalHeaders headers = additionalHeaders;

    if (!externalRequest)
    {
        if (Http::gameSessionID.length() > 0 && headers.end() == headers.find(kGameSessionHeaderKey))
        {
            headers[kGameSessionHeaderKey] = Http::gameSessionID;
        }

        if (Http::gameID.length() > 0 && headers.end() == headers.find(kGameIdHeaderKey))
        {
            headers[kGameIdHeaderKey] = Http::gameID;
        }

        if (Http::placeID.length() > 0 && headers.end() == headers.find(kPlaceIdHeaderKey))
        {
            headers[kPlaceIdHeaderKey] = Http::placeID;
        }

        if (Http::requester.length() > 0 && headers.end() == headers.find(kRequesterHeaderKey))
        {
            headers[kRequesterHeaderKey] = Http::requester;
        }

		if (useCurlHttpImpl && !forceNativeHttp)
        {
            if (externalRequest && headers.end() == headers.find(kAccessHeaderKey))
            {
                headers[kAccessHeaderKey] = "UserRequest";
            }
            else if (!Http::accessKey.empty() && headers.end() == headers.find(kAccessHeaderKey))
            {
                headers[kAccessHeaderKey] = Http::accessKey;
            }
        }
    }

#if defined(RBX_PLATFORM_DURANGO)
	if (!useCurlHttpImpl || forceNativeHttp)
	{
        if (url.find("http://") == 0) // starts with http?
        {
            dprintf( "WARN HTTP: %s\n", url.c_str() );
        }

		try
		{
			httpGetPostXbox(isPost, dataStream, contentType, compressData, headers, externalRequest, cachePolicy, response);
			if (recordStatistics)
			{
				HTTPStatistics::success(httpTimer.delta().msec(), url.c_str(), response.size());
			}
		}
		catch (const RBX::http_status_error& e)
		{
			if (recordStatistics)
			{
				HTTPStatistics::failure(httpTimer.delta().msec(), url.c_str(), response.size(), e.what(), e.statusCode);
			}
			throw;
		}
		catch (const std::exception& e)
		{
			if (recordStatistics)
			{
				HTTPStatistics::failure(httpTimer.delta().msec(), url.c_str(), response.size(), e.what());
			}
			throw;
		}
		return;
	}
#endif // ifdef _WIN32

    HttpPlatformImpl::HttpOptions httpOpts(url, externalRequest, cachePolicy, connectTimeoutMillis, responseTimeoutMillis);
    if (isPost)
    {
        httpOpts.setPostData(&dataStream, compressData);
    }
    httpOpts.setHeaders(&contentType, &headers);

    try
    {
        HttpPlatformImpl::perform(httpOpts, response);
        if (recordStatistics)
        {
            HTTPStatistics::success(httpTimer.delta().msec(), url.c_str(), response.size());
        }
    }
    catch (const RBX::http_status_error& e)
    {
		bool didMakeNativeRequest = doHttpGetPostWithNativeFallbackForReporting(isPost, dataStream, contentType, compressData, additionalHeaders,
				externalRequest, response);

        if (recordStatistics)
        {
            HTTPStatistics::failure(httpTimer.delta().msec(), url.c_str(), response.size(), e.what(), e.statusCode);
        }
		if(!didMakeNativeRequest) throw;
    }
    catch (const std::exception& e)
    {
        if (recordStatistics)
        {
            HTTPStatistics::failure(httpTimer.delta().msec(), url.c_str(), response.size(), e.what());
        }
        throw;
    }
}

void Http::post(std::istream& input, const std::string& contentType,
                bool compress, std::string& response, bool externalRequest)
{
    httpGetPost(true, input, contentType, compress, additionalHeaders, externalRequest, response);
}

static void doGet(Http http, bool externalRequest,
                  boost::function<void(std::string*, std::exception*)> handler)
{
    std::string response;
    try
    {
        http.get(response, externalRequest);
    }
    catch (RBX::base_exception& ex)
    {
        handler(0, &ex);
        return;
    }
    handler(&response, 0);
}

void Http::get(
    boost::function<void(std::string*, std::exception*)> handler,
    bool externalRequest)
{
    threadPool->schedule(boost::bind(&doGet, *this, externalRequest, handler));
}

static void doPostStream(std::string url, boost::shared_ptr<std::istream> input,
						 const std::string& contentType, bool compress, bool externalRequest, bool recordStatistics,
						 boost::function<void(std::string *, std::exception*)> handler)
{
	std::string response;
	try
	{
		Http http(url);
        http.recordStatistics = recordStatistics;
		http.post(*input, contentType, compress, response, externalRequest);
	}
	catch (RBX::base_exception &ex)
	{
		handler(0, &ex);
		return;
	}
	handler(&response, 0);
}

static void doPost(std::string url, std::string input,
                   const std::string& contentType, bool compress, bool externalRequest, bool recordStatistics,
                   boost::function<void(std::string*, std::exception*)> handler)
{
    shared_ptr<std::istream> inputStream(new std::istringstream(input));
    doPostStream(url, inputStream, contentType, compress,
        externalRequest, recordStatistics, handler);
}

void Http::post(const std::string& input, const std::string& contentType,
                bool compress,
                boost::function<void(std::string*, std::exception*)> handler,
                bool externalRequest)
{
    threadPool->schedule(
        boost::bind(&doPost, url, input, contentType, compress,
                    externalRequest, recordStatistics, handler));
}

void Http::post(boost::shared_ptr<std::istream> input,
                const std::string& contentType, bool compress,
                boost::function<void(std::string *, std::exception*)> handler,
                bool externalRequest)
{
    threadPool->schedule(
        boost::bind(&doPostStream, url, input, contentType, compress,
                    externalRequest, recordStatistics, handler));
}

void Http::get(std::string& response, bool externalRequest)
{
    Time startTime = Time::now<Time::Fast>();

    std::istringstream dummy;
    try
    {
        httpGetPost(false, dummy, "", false, additionalHeaders, externalRequest, response);
        if (!alternateUrl.empty())
            ++cdnSuccessCount;
    }
	catch (RBX::base_exception& e)
    {
        if (shouldRetry)
        {
			shouldRetry = false;

            response.clear();
            const double elapsed = (Time::now<Time::Fast>() - startTime).seconds();
            if (!alternateUrl.empty())
            {
                ++cdnFailureCount;
                lastCdnFailureTimeSpan = elapsed;
                RBX::StandardOut::singleton()->printf(RBX::MESSAGE_WARNING,
                                                      "httpGet %s failed. Trying alternate %s. Error: %s.  Elapsed time: %g",
                                                      url.c_str(), alternateUrl.c_str(), e.what(), elapsed);
                try
                {
                    httpGetPost(false, dummy, "", false, additionalHeaders, externalRequest, response);
                    ++alternateCdnSuccessCount;
                }
				catch (RBX::base_exception&)
                {
                    ++alternateCdnFailureCount;
                    throw;
                }
            }
            else
            {
                RBX::StandardOut::singleton()->printf(RBX::MESSAGE_WARNING,
                                                      "httpGet %s failed. Trying again. Error: %s.  Elapsed time: %g",
                                                      url.c_str(), e.what(), elapsed);

                httpGetPost(false, dummy, "", false, additionalHeaders, externalRequest, response);
            }
        }
        else
        {
            // rethrow the existing exception
            throw;
        }
    }
}

bool Http::isScript(const char* url)
{
    static const char* javascript = "javascript:";
    static const char* jscript = "jscript:";
    return
        0 == strncmp(url, javascript, strlen(javascript)) ||
        0 == strncmp(url, jscript, strlen(jscript));
}

bool Http::isMoneySite(const char* url)
{
    RBX::Url parsed = RBX::Url::fromString(url);

    return parsed.hasHttpScheme() &&
        (parsed.isSubdomainOf("paypal.com") || parsed.isSubdomainOf("rixty.com"));
}

// This allows just roblox domains, not roblox trusted domains like facebook.
bool Http::isStrictlyRobloxSite(const char* url)
{
    RBX::Url parsed = RBX::Url::fromString(url);

    // include configured base URL domain in the whitelist as well
    const std::string baseUrl = ::GetBaseURL();
    if (!baseUrl.empty())
    {
        RBX::Url baseParsed = RBX::Url::fromString(baseUrl);
        std::string baseHost = baseParsed.host();
        if (baseHost.size() > 4 && baseHost.substr(0, 4) == "www.")
            baseHost = baseHost.substr(4);

        if (!baseHost.empty() && parsed.isSubdomainOf(baseHost.c_str()))
        {
            return true;
        }
    }

    // baseurl is empty
    return parsed.isSubdomainOf("roblox.com") || parsed.isSubdomainOf("robloxlabs.com");
}

bool Http::isRobloxSite(const char* url)
{
    RBX::Url parsed = RBX::Url::fromString(url);
    if (!parsed.hasHttpScheme())
    {
        return false;
    }

    // also allow any subdomain of the configured base URL host
    bool isRoblox = parsed.isSubdomainOf("roblox.com") || parsed.isSubdomainOf("robloxlabs.com");
    const std::string baseUrl = ::GetBaseURL();
    if (!baseUrl.empty())
    {
        RBX::Url baseParsed = RBX::Url::fromString(baseUrl);
        std::string baseHost = baseParsed.host();
        if (baseHost.size() > 4 && baseHost.substr(0, 4) == "www.")
            baseHost = baseHost.substr(4);

        if (!baseHost.empty() && parsed.isSubdomainOf(baseHost.c_str()))
        {
            isRoblox = true;
        }
    }

    const bool isFacebook =
        ("login.facebook.com" == parsed.host()
            && parsed.pathEqualsCaseInsensitive("/login.php")) ||
        ("ssl.facebook.com" == parsed.host()
            && parsed.pathEqualsCaseInsensitive("/connect/uiserver.php")) ||
        ("www.facebook.com" == parsed.host()
            && (parsed.pathEqualsCaseInsensitive("/connect/uiserver.php")
                || parsed.pathEqualsCaseInsensitive("/logout.php")));

    const bool isYoutube =
        ("www.youtube.com" == parsed.host()
            && (parsed.pathEqualsCaseInsensitive("/auth_sub_request")
            || parsed.pathEqualsCaseInsensitive("/signin")
            || parsed.pathEqualsCaseInsensitive("/issue_auth_sub_token"))) ||
        ("uploads.gdata.youtube.com" == parsed.host());

    const bool isGoogle =
        ("www.google.com" == parsed.host()
            && parsed.pathEqualsCaseInsensitive("/accounts/serviceloginauth")) ||
        ("accounts.google.com" == parsed.host()
            && parsed.pathEqualsCaseInsensitive("/serviceloginauth"));

    return isRoblox || isFacebook || isYoutube || isGoogle;
}

bool Http::trustCheckBrowser(const char* url)
{
    return trustCheck(url) || isMoneySite(url);
}

bool Http::isExternalRequest(const char* url)
{
    RBX::Url parsed = RBX::Url::fromString(url);
    if (!parsed.hasHttpScheme())
    {
        return false;
    }

    // treat requests as external only if they are not subdomains of roblox, robloxlabs,
    // or the configured base url host
    bool isRobloxDomain = parsed.isSubdomainOf("roblox.com") || parsed.isSubdomainOf("robloxlabs.com");
    const std::string baseUrl = ::GetBaseURL();
    if (!baseUrl.empty())
    {
        RBX::Url baseParsed = RBX::Url::fromString(baseUrl);
        std::string baseHost = baseParsed.host();
        if (baseHost.size() > 4 && baseHost.substr(0, 4) == "www.")
            baseHost = baseHost.substr(4);

        if (!baseHost.empty() && parsed.isSubdomainOf(baseHost.c_str()))
        {
            isRobloxDomain = true;
        }
    }

    return !isRobloxDomain;
}

void Http::setProxy(const std::string& host, long port)
{
    HttpPlatformImpl::setProxy(host, port);
}

bool Http::trustCheck(const char* url, bool externalRequest)
{
    if (externalRequest)
    {
        return isExternalRequest(url);
    }

    if (std::string("about:blank") == url ||
        isRobloxSite(url) ||
        isScript(url))
    {
        return true;
    }

    return false;
}

bool Http::doHttpGetPostWithNativeFallbackForReporting(bool isPost, std::istream& dataStream, const std::string& contentType, bool compressData, const HttpAux::AdditionalHeaders& additionalHeaders, bool allowExternal, std::string& response)
{
    return false;
}

void Http::applyAdditionalHeaders(RBX::HttpAux::AdditionalHeaders& outHeaders)
{
	for (RBX::HttpAux::AdditionalHeaders::const_iterator itr = outHeaders.begin(); itr != outHeaders.end(); ++itr)
	{
		this->additionalHeaders[itr->first] = itr->second;
	}
}
