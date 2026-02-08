/**
 * RobloxTutorials.cpp
 * Copyright (c) 2014 ROBLOX Corp. All Rights Reserved.
 */

#include "stdafx.h"
#include "RobloxTutorials.h"

// Qt Headers
#include <QWidget>
#include <QGridLayout>
#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QKeySequence>
#include <QSize>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QUrlQuery>

// Roblox Headers
#include "v8datamodel/FastLogSettings.h"

// Roblox Studio Headers
#include "RobloxBrowser.h"
#include "RobloxMainWindow.h"
#include "RobloxWebPage.h"
#include "QtUtilities.h"
#include "UpdateUIManager.h"
#include "qdesktopservices.h"

#define COLLAPSED_SIZE 142
#define EXPAND_THRESHOLD 300

FASTFLAGVARIABLE(StudioShowTutorialsByDefault, false);
FASTFLAGVARIABLE(StudioTutorialSeeAll, false);
FASTSTRINGVARIABLE(StudioTutorialsSeeAllUrl, "http://wiki.roblox.com/index.php?title=AllTutorials");
FASTSTRINGVARIABLE(StudioTutorialsUrl, "http://wiki.roblox.com/index.php?title=StudioTutorials&studiomode=true");
FASTSTRINGVARIABLE(StudioTutorialsTOCUrl, "http://wiki.roblox.com/index.php?title=Studio_Tutorials_Landing&studiomode=true");

RobloxTutorials::RobloxTutorials()
: m_pWebView(NULL)
, m_pWrapperWidget(NULL)
, m_pWebContentsView(NULL)
, m_firstPaint(true)
, m_firstClick(true)
{
	if (!FFlag::StudioShowTutorialsByDefault)
		return;

	m_pWrapperWidget = new QWidget(this);

	setupWebView(m_pWrapperWidget);

	QVBoxLayout* layout = new QVBoxLayout(m_pWrapperWidget);
	layout->addWidget(m_pWebContentsView);
	layout->addWidget(m_pWebView);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	setLayout(layout);
}

void RobloxTutorials::handleHomeClicked()
{
	m_pWebView->load(QUrl(FString::StudioTutorialsUrl.c_str()));
}

void RobloxTutorials::updateButtons()
{
	m_pStopButton->setEnabled(m_pWebView->pageAction(QWebEnginePage::Stop)->isEnabled());
	m_pReloadButton->setEnabled(m_pWebView->pageAction(QWebEnginePage::Reload)->isEnabled());
	m_pBackButton->setEnabled(m_pWebView->pageAction(QWebEnginePage::Back)->isEnabled());
	m_pForwardButton->setEnabled(m_pWebView->pageAction(QWebEnginePage::Forward)->isEnabled());
}

void RobloxTutorials::resizeWidget()
{
	QWidget* dockWidget = dynamic_cast<QWidget*>(this->parent());
	setMaximumHeight(QWIDGETSIZE_MAX);
	dockWidget->setMinimumHeight(COLLAPSED_SIZE);
	dockWidget->setMaximumHeight(QWIDGETSIZE_MAX);
}

void RobloxTutorials::disableLinks(const QUrl& url)
{
}

void RobloxTutorials::urlChanged(const QUrl& url)
{
	QString urlString = url.toString();

	int strCmp = urlString.compare(QString(FString::StudioTutorialsTOCUrl.c_str()));
	if (strCmp == 0) {
		m_firstClick = true;
	}
}

void RobloxTutorials::linkClicked(const QUrl& url)
{
	if (m_firstClick)
	{
		m_firstClick = false;
		m_pWebContentsView->load(QUrl(FString::StudioTutorialsUrl.c_str()));
	}
	else
	{
		// Check if "See All" link is clicked. If so, launch browser and don't navigate to new page in panel
		QString urlString = url.toString();
		if (FFlag::StudioTutorialSeeAll && urlString.endsWith("Link="))
		{
			QUrl allTutorials(FString::StudioTutorialsSeeAllUrl.c_str(), QUrl::TolerantMode);
			QDesktopServices::openUrl(allTutorials);
			return;
		}

		if (height() < EXPAND_THRESHOLD)
		{
			QWidget* dockWidget = dynamic_cast<QWidget*>(this->parent());
			dockWidget->setFixedHeight(UpdateUIManager::Instance().getMainWindow().size().height() / 2);

			QMetaObject::invokeMethod(this, "resizeWidget", Qt::QueuedConnection);
		}

		QUrl studioModeUrl = url;
		QUrlQuery query(studioModeUrl);
		if (!query.hasQueryItem("studiomode"))
			query.addQueryItem("studiomode", "true");
		studioModeUrl.setQuery(query);
		m_pWebView->load(studioModeUrl);
	}
}

void RobloxTutorials::setupWebView(QWidget *wrapperWidget)
{
	m_pWebView = new RobloxBrowser(wrapperWidget);
	m_pWebView->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
	m_pWebView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pWebView->setPage(new RobloxWebPage(wrapperWidget));

	m_pWebContentsView = new RobloxBrowser(wrapperWidget);
	m_pWebContentsView->setMaximumSize(QWIDGETSIZE_MAX, COLLAPSED_SIZE);
	m_pWebContentsView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_pWebContentsView->setPage(new RobloxWebPage(wrapperWidget));
	m_pWebContentsView->page()->runJavaScript("document.body.style.overflow = 'hidden';");

	setMaximumHeight(COLLAPSED_SIZE);

	// This is to listen to the url changed event for the main tutorial frame. When the url changes we want to disable
	// history so a user can't navigate back
	connect(m_pWebContentsView, SIGNAL(urlChanged(const QUrl&)), this, SLOT(urlChanged(const QUrl&)));
	connect(m_pWebContentsView->page(), SIGNAL(linkClicked(const QUrl&)), this, SLOT(linkClicked(const QUrl&)));

	m_pWebContentsView->load(QUrl(FString::StudioTutorialsTOCUrl.c_str()));

	QWebEngineSettings *globalSetting = QWebEngineProfile::defaultProfile()->settings();

	globalSetting->setAttribute(QWebEngineSettings::AutoLoadImages, true);

	globalSetting->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
	globalSetting->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);

#ifdef _WIN32
	globalSetting->setAttribute(QWebEngineSettings::PluginsEnabled, true);
#endif
}
