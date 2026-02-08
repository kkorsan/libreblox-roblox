/**
 * RobloxToolBox.cpp
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#include "stdafx.h"
#include "RobloxToolBox.h"

// Qt Headers
#include <QGridLayout>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

// Roblox Headers
#include "v8datamodel/DataModel.h"
#include "v8datamodel/InsertService.h"
#include "v8datamodel/ContentProvider.h"
#include "util/standardout.h"
#include "network/Players.h"

// Roblox Studio Headers
#include "AuthenticationHelper.h"
#include "RbxWorkspace.h"
#include "RobloxCookieJar.h"
#include "RobloxNetworkAccessManager.h"
#include "RobloxSettings.h"

FASTFLAG(WebkitLocalStorageEnabled);
FASTFLAG(StudioEnableWebKitPlugins);

RobloxToolBox::RobloxToolBox()
: m_pWorkspace()
, m_pWebView(NULL)
, reloadView(false)
{
	QGridLayout *layout = new QGridLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

    setMaximumWidth(250);

	setupWebView(this);
	layout->addWidget(m_pWebView, 0, 0);
	setLayout(dynamic_cast<QLayout*>(layout));

    setMaximumWidth(QWIDGETSIZE_MAX);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

	setMinimumWidth(285);
}

void RobloxToolBox::setupWebView(QWidget *wrapperWidget)
{
	m_pWebView = new RobloxBrowser(wrapperWidget);
	m_pWebPage = new RobloxWebPage(wrapperWidget);

	m_pWebView->setPage(m_pWebPage);

	QWebEngineSettings *globalSetting = QWebEngineProfile::defaultProfile()->settings();

	globalSetting->setAttribute(QWebEngineSettings::AutoLoadImages, true);
	globalSetting->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
	globalSetting->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
	globalSetting->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);

#ifdef _WIN32
    if (FFlag::StudioEnableWebKitPlugins)
        globalSetting->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    else
        globalSetting->setAttribute(QWebEngineSettings::PluginsEnabled, false);
#endif

	/// Keep all this for now, later on we should remove it depending on bare minimum required.
	globalSetting->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
	globalSetting->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);

	if(FFlag::WebkitLocalStorageEnabled)
		globalSetting->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);

	m_urlString = QString("%1/IDE/ClientToolbox.aspx").arg(RobloxSettings::getBaseURL());

	connect(&AuthenticationHelper::Instance(), SIGNAL(authenticationChanged(bool)), this, SLOT(onAuthenticationChanged(bool)));
}


void RobloxToolBox::setDataModel(boost::shared_ptr<RBX::DataModel> pDataModel)
{
	if(m_pDataModel == pDataModel)
		return;

    bool firstTime = false;
    m_pDataModel = pDataModel;

    if (!m_pWorkspace)
    {
        m_pWorkspace.reset(new RbxWorkspace(this, m_pDataModel ? m_pDataModel.get() : NULL));
        firstTime = true;
    }
    else
    {
        m_pWorkspace->setDataModel(pDataModel.get());
    }

    if (!m_pDataModel)
	{
		setEnabled(false);
		return;
	}

	setEnabled(true);

    if (firstTime)
	{
        m_pWebView->load(m_urlString);
	}
	else if (reloadView)
	{
		m_pWebPage->triggerAction(QWebEnginePage::Reload);
	}
	reloadView = false;

	update();
}

void RobloxToolBox::initJavascript()
{
	// Removed for QtWebEngine migration
}

QString RobloxToolBox::getTitleFromUrl(const QString &urlString)
{
	// Simplified for QtWebEngine migration
	return QString("");
}

void RobloxToolBox::onAuthenticationChanged(bool)
{
	if (m_pDataModel)
		m_pWebPage->triggerAction(QWebEnginePage::Reload);
	else
		reloadView = true;
}

void RobloxToolBox::loadUrl(const QString url)
{
	setEnabled(true);
	m_urlString = url;
	m_pWebView->load(url);
}
