#include "stdafx.h"

#include "CountersClient.h"
#include "RobloxServicesTools.h"

#include <set>
#include <string>

// Use cURL for HTTP requests
#include <curl/curl.h>

std::set<std::wstring> CountersClient::_events;

CountersClient::CountersClient(std::string baseUrl, std::string key, simple_logger<wchar_t>* logger) :
_logger(logger)
{
	_url = GetCountersMultiIncrementUrl(baseUrl, key);
	//TODO use LOG_ENTRY macros here
	if (logger)
		logger->write_logentry("CountersClient::CountersClient baseUrl=%s, url=%s", baseUrl.c_str(), _url.c_str());
}

void CountersClient::registerEvent(std::wstring eventName, bool fireImmediately)
{
	if(fireImmediately)
	{
		std::set<std::wstring> eventList;
		eventList.insert(eventName);
		reportEvents(eventList);
		return;
	}

	std::set<std::wstring>::iterator i = _events.find(eventName);
	if (_events.end() == i)
	{
		_events.insert(eventName);
	}
}

void CountersClient::reportEvents()
{
	reportEvents(_events);
}

void CountersClient::reportEvents(std::set<std::wstring> &events)
{
	if (events.empty())
	{
		return;
	}

	std::string postData = "counterNamesCsv=";
	bool append = false;
	for (std::set<std::wstring>::iterator i = events.begin();i != events.end();i ++)
	{
		if (append)
		{
			postData += ",";
		}

		postData += convert_w2s(*i);

		append = true;
	}

	events.clear();

	// Use cURL to perform the HTTP POST
	CURL* curl = curl_easy_init();
	if(curl)
	{
		struct curl_slist* headers = NULL;
		headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

		curl_easy_setopt(curl, CURLOPT_URL, _url.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)postData.length());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Roblox/Curl");

		long timeout_ms = 5 * 1000;
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);

		CURLcode res = curl_easy_perform(curl);

		// Optionally log the response or error if needed
		if (res != CURLE_OK && _logger)
		{
			_logger->write_logentry("CountersClient::reportEvents curl failed: %s", curl_easy_strerror(res));
		}

		// Cleanup
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	}
}
