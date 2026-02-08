/**
 * RobloxUser.cpp
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#include "stdafx.h"
#include <future>
#include "RobloxUser.h"

// Roblox headers
#include "rbx/Debug.h"

// Roblox Studio headers
#include "RobloxCookieJar.h"
#include "RobloxNetworkAccessManager.h"
#include "RobloxNetworkReply.h"
#include "RobloxSettings.h"
#include "AuthenticationHelper.h"

#include "util/RobloxGoogleAnalytics.h"
#include "v8datamodel/Stats.h"

RobloxUser& RobloxUser::singleton()
{
    static RobloxUser user;
	return user;
}

RobloxUser::RobloxUser()
	: m_webKitUserId(-1)
{
    connect(&AuthenticationHelper::Instance(), SIGNAL(authenticationChanged(bool)), this, SLOT(onAuthenticationChanged(bool)));
}

RobloxUser::~RobloxUser()
{
}

void RobloxUser::init()
{
	if (m_webKitUserId == -1)
		getWebkitUserId();
}

// Keep this user up to sync on auth changes
void RobloxUser::onAuthenticationChanged(bool)
{
	m_webKitUserId = -1;
	getWebkitUserId();
}

void RobloxUser::currentUserReplied(RBX::HttpFuture future)
{
	try {
		QString webKitUserIDStr(QString::fromStdString(future.get()));
		m_webKitUserId = webKitUserIDStr.toInt();
		RBX::Analytics::setUserId(m_webKitUserId);

		// TODO: remove
		RBX::RobloxGoogleAnalytics::setUserID(m_webKitUserId);
	}
	catch(std::exception&)
	{
		m_webKitUserId = 0;
	}
}

void RobloxUser::getWebkitUserId()
{
	std::string url(AuthenticationHelper::getLoggedInUserUrl().toStdString());
    RBX::HttpFuture future = RBX::HttpAsync::get(url);
    currentUserReplied(future);
    m_webKitUserIDQueryValid = true;
}

int RobloxUser::getUserId()
{
	if(m_webKitUserId == -1)
	{
		// make sure we are authenticated before querying user id		
		AuthenticationHelper::Instance().waitForHttpAuthentication();
		if (!m_webKitUserIDQueryValid)
			getWebkitUserId();		

	}

	return m_webKitUserId;
}
