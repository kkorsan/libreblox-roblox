#include "stdafx.h"

#include "RobloxServicesTools.h"
#include "format_string.h"
#include "util/URL.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <sstream>


#ifdef RBX_PLATFORM_DURANGO
#define DEFAULT_URL_SCHEMA "https"
#else
#define DEFAULT_URL_SCHEMA "http"
#endif

std::string trim_trailing_slashes(const std::string &path)
{
	size_t i = path.find_last_not_of('/');
	return path.substr(0, i+1);
}

static std::string BuildGenericApiUrl(const std::string &baseUrl, const std::string &serviceNameIn, const std::string &path, const std::string &key, const char* scheme = "https")
{
    using RBX::Url;

    std::string serviceName(serviceNameIn);

    // Helper: split host into host (no port) and port (including ':') parts.
    auto splitHostPort = [](const std::string& hostWithPort) -> std::pair<std::string,std::string>
    {
        if (hostWithPort.empty()) return { "", "" };

        // IPv6 literal in brackets: [::1]:8080
        if (hostWithPort.front() == '[')
        {
            auto closeBracket = hostWithPort.find(']');
            if (closeBracket != std::string::npos)
            {
                if (closeBracket + 1 < hostWithPort.size() && hostWithPort[closeBracket + 1] == ':')
                    return { hostWithPort.substr(0, closeBracket + 1), hostWithPort.substr(closeBracket + 1) };
                return { hostWithPort, "" };
            }
        }

        // For typical host:port split, take last ':' as separator, but ignore if it's part of IPv6 without brackets.
        auto pos = hostWithPort.rfind(':');
        if (pos == std::string::npos)
            return { hostWithPort, "" };

        // If there is another ':' earlier, assume it's IPv6 without brackets - treat whole string as host (no port).
        if (hostWithPort.find(':') != pos)
            return { hostWithPort, "" };

        return { hostWithPort.substr(0, pos), hostWithPort.substr(pos) }; // port includes ':'
    };

    // Helper: trim leading '/' from path to avoid doubling slashes
    auto trimLeadingSlash = [](const std::string& p) -> std::string {
        if (p.empty()) return std::string();
        if (p.front() == '/') return p.substr(1);
        return p;
    };

    	// Parse the base URL using Url class; fall back to raw baseUrl when parsing fails.
    	Url parsed = Url::fromString(baseUrl.c_str());
    	// Prefer explicit port parsed by Url; if present, append it when creating hostWithPort.
    	// Url::host() returns the host without the port (port() is separate), so preserve parsed port if available.
    	std::string hostWithPort = parsed.host();
    	if (!parsed.port().empty())
    		hostWithPort += ":" + parsed.port();
    	std::string schemeToUse = parsed.scheme().empty() ? std::string(scheme) : parsed.scheme();

    if (hostWithPort.empty())
    {
        // Try to extract host from raw baseUrl (no scheme provided)
        // baseUrl can be like "localhost:8080" or "www.example.com"
        // Find first '/' to strip path
        size_t slashPos = baseUrl.find('/');
        if (slashPos != std::string::npos)
            hostWithPort = baseUrl.substr(0, slashPos);
        else
            hostWithPort = baseUrl;
    }

    auto hostPortPair = splitHostPort(hostWithPort);
    std::string hostNoPort = hostPortPair.first;
    std::string portPart = hostPortPair.second; // either empty or like ":8080"

    // Work with lowercase host for domain checks
    std::string hostNoPortLower = hostNoPort;
    std::transform(hostNoPortLower.begin(), hostNoPortLower.end(), hostNoPortLower.begin(), ::tolower);

    // Roblox-specific domains handling (preserve historical behavior)
    const std::vector<std::string> knownRbx = { "roblox.com", "robloxlabs.com" };
    std::string rbxDomainMatched;
    for (const auto& candidate : knownRbx)
    {
        if (boost::algorithm::iends_with(hostNoPortLower, candidate))
        {
            rbxDomainMatched = candidate;
            break;
        }
    }

    // Ensure serviceName ends with '.' when non-empty
    if (!serviceName.empty() && serviceName.back() != '.')
        serviceName.push_back('.');

    std::string cleanPath = trimLeadingSlash(path);

    std::string url;

    if (!rbxDomainMatched.empty())
    {
        // Derive subdomain (portion preceding the known RBX domain)
        std::string subdomain;
        if (hostNoPortLower.size() > rbxDomainMatched.size())
        {
            subdomain = hostNoPortLower.substr(0, hostNoPortLower.size() - rbxDomainMatched.size());
            // remove possible trailing '.'
            if (!subdomain.empty() && subdomain.back() == '.')
                subdomain.erase(subdomain.size() - 1);
        }

        // treat empty or common mobile/www subdomains as production
        if (subdomain.empty() || subdomain == "www" || subdomain == "m")
        {
            // production
            url = format_string("%s://%sapi.%s/%s/?apiKey=%s", schemeToUse.c_str(), serviceName.c_str(), rbxDomainMatched.c_str(), cleanPath.c_str(), key.c_str());
        }
        else
        {
            // Special-case sitetest3 anywhere in subdomain (match original behavior)
            if (subdomain.find("sitetest3") != std::string::npos)
            {
                url = format_string("%s://%sapi.sitetest3.%s/%s/?apiKey=%s", schemeToUse.c_str(), serviceName.c_str(), rbxDomainMatched.c_str(), cleanPath.c_str(), key.c_str());
            }
            else
            {
                // strip leading "www." if present
                if (boost::algorithm::istarts_with(subdomain, "www."))
                    subdomain = subdomain.substr(4);

                // strip leading "m." if present
                if (boost::algorithm::istarts_with(subdomain, "m."))
                    subdomain = subdomain.substr(2);

                // build api.<subdomain>.<rbxDomainMatched>
                url = format_string("%s://%sapi.%s.%s/%s/?apiKey=%s", schemeToUse.c_str(), serviceName.c_str(), subdomain.c_str(), rbxDomainMatched.c_str(), cleanPath.c_str(), key.c_str());
            }
        }
    }
    else
    {
        // Generic domain handling: preserve host and port. For unknown domains, place "api." before the host.
        // For common hosts like www.example.com or m.example.com strip the leading 'www.' / 'm.' so we end up with api.example.com
        // Example: baseUrl "http://localhost:8080" with serviceName "ephemeralcounters" =>
        // "http://ephemeralcounters.api.localhost:8080/..."
        std::string hostTrim = hostNoPort;
        if (boost::algorithm::istarts_with(hostTrim, "www."))
        {
            hostTrim = hostTrim.substr(4);
        }
        else if (boost::algorithm::istarts_with(hostTrim, "m."))
        {
            hostTrim = hostTrim.substr(2);
        }

        // If this is a localhost / loopback host or an IP address, do not prefix with api or serviceName.
        // Use the host[:port] directly so URLs like http://localhost:3001/... or http://192.168.0.1/... are preserved.
        auto isIpAddress = [](const std::string& h) {
            if (h.empty()) return false;
            // IPv6 literal
            if (h.front() == '[' && h.back() == ']') return true;

            // Simple IPv4 check: contains only digits and dots, and has exactly 3 dots.
            int dots = 0;
            for (char c : h) {
                if (c == '.') dots++;
                else if (c < '0' || c > '9') return false;
            }
            return dots == 3;
        };

        if (parsed.isLocalhost() || isIpAddress(hostTrim))
        {
            std::string hostAndPort = hostTrim + portPart;
            url = format_string("%s://%s/%s/?apiKey=%s", schemeToUse.c_str(), hostAndPort.c_str(), cleanPath.c_str(), key.c_str());
        }
        else
        {
            std::string domainWithPort = hostTrim + portPart;
            if (!serviceName.empty())
            {
                url = format_string("%s://%sapi.%s/%s/?apiKey=%s", schemeToUse.c_str(), serviceName.c_str(), domainWithPort.c_str(), cleanPath.c_str(), key.c_str());
            }
            else
            {
                // no serviceName: just api.<domain>
                url = format_string("%s://api.%s/%s/?apiKey=%s", schemeToUse.c_str(), domainWithPort.c_str(), cleanPath.c_str(), key.c_str());
            }
        }
    }

    return url;
}

std::string GetCountersUrl(const std::string &baseUrl, const std::string &key)
{
	return BuildGenericApiUrl(baseUrl, "", "v1.1/Counters/Increment", key, DEFAULT_URL_SCHEMA);
}

std::string GetCountersMultiIncrementUrl(const std::string &baseUrl, const std::string &key)
{
    return BuildGenericApiUrl(baseUrl, "", "v1.0/MultiIncrement", key, DEFAULT_URL_SCHEMA);
}

std::string GetSettingsUrl(const std::string &baseUrl, const std::string &group, const std::string &key)
{
    return BuildGenericApiUrl(baseUrl, "", format_string("Setting/QuietGet/%s", group.c_str()), key, DEFAULT_URL_SCHEMA);
}

std::string GetSecurityKeyUrl(const std::string &baseUrl, const std::string &key)
{
	return BuildGenericApiUrl(baseUrl, "", "api/security/rcc/security-keys", key);
}

std::string GetSecurityKeyUrl2(const std::string &baseUrl, const std::string &key)
{
	return BuildGenericApiUrl(baseUrl, "", "api/security/rcc/security-keys", key);
}

// used by bootstrapper
std::string GetClientVersionUploadUrl(const std::string &baseUrl, const std::string &key)
{
	return BuildGenericApiUrl(baseUrl, "", "GetCurrentClientVersionUpload", key);
}

std::string GetPlayerGameDataUrl(const std::string &baseurl, int userId, const std::string &key)
{
	return BuildGenericApiUrl(baseurl, "", format_string("/game/players/%d", userId), key);
}

std::string GetWebChatFilterURL(const std::string& baseUrl, const std::string& key)
{
	return BuildGenericApiUrl(baseUrl, "", "/moderation/filtertext", key);
}

std::string GetMD5HashUrl(const std::string &baseUrl, const std::string &key)
{
	return BuildGenericApiUrl(baseUrl, "", "api/security/rcc/md5-hashes", key);
}

std::string GetMemHashUrl(const std::string &baseUrl, const std::string &key)
{
	return BuildGenericApiUrl(baseUrl, "", "GetAllowedMemHashes", key);
}

std::string GetGridUrl(const std::string &anyUrl, bool changeToDataDomain)
{
    std::string url = trim_trailing_slashes(anyUrl);
	if (changeToDataDomain)
		url = ReplaceTopSubdomain(url, "data");

    url = url + "/Error/Grid.ashx";
    return url;
}

std::string GetDmpUrl(const std::string &anyUrl, bool changeToDataDomain)
{
    std::string url = trim_trailing_slashes(anyUrl);
	if (changeToDataDomain)
		url = ReplaceTopSubdomain(url, "data");

	url = url + "/Error/Dmp.ashx";
    return url;
}

std::string GetBreakpadUrl(const std::string &anyUrl, bool changeToDataDomain)
{
    std::string url = trim_trailing_slashes(anyUrl);
	if (changeToDataDomain)
		url = ReplaceTopSubdomain(url, "data");

	url = url + "/Error/Breakpad.ashx";
    return url;
}

std::string ReplaceTopSubdomain(const std::string& anyUrl, const char* newTopSubDoman)
{
    std::string result(anyUrl);
    std::size_t foundPos = result.find("www.");
    if (foundPos != std::string::npos)
    {
        result.replace(foundPos, 3, newTopSubDoman);
    }
    else if ((foundPos = result.find("m.")) != std::string::npos)
    {
        result.replace(foundPos, 1, newTopSubDoman);
    }
    return result;
}

std::string BuildGenericPersistenceUrl(const std::string& baseUrl, const std::string &servicePath)
{
    std::string constructedURLDomain(baseUrl);
    std::string constructedServicePath(servicePath);

	constructedURLDomain = ReplaceTopSubdomain(constructedURLDomain, "gamepersistence");
    if (!boost::algorithm::ends_with(constructedURLDomain, "/"))
    {
        constructedURLDomain.append("/");
    }
    return constructedURLDomain + constructedServicePath + "/" ;
}

std::string BuildGenericGameUrl(const std::string& baseUrl, const std::string &servicePath)
{
    std::string constructedURLDomain(baseUrl);
    std::string constructedServicePath(servicePath);

	constructedURLDomain = ReplaceTopSubdomain(constructedURLDomain, "assetgame");
    if (!boost::algorithm::ends_with(constructedURLDomain, "/"))
    {
        constructedURLDomain.append("/");
    }
    if (boost::algorithm::starts_with(constructedServicePath, "/"))
    {
        constructedServicePath.erase(constructedServicePath.begin());
    }
    return constructedURLDomain + constructedServicePath;
}
