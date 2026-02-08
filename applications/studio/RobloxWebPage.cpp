/**
 * RobloxWebPage.cpp
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#include "stdafx.h"
#include "RobloxWebPage.h"
#include "RobloxNetworkAccessManager.h"
#include "util/RobloxGoogleAnalytics.h"

// Qt Headers
#include <QObject>
#include <QDesktopServices>
#include <QNetworkReply>
#include <QKeyEvent>
#include <QCoreApplication>

QString RobloxWebPage::userAgentForUrl(const QUrl &url) const 
{	
	return RobloxNetworkAccessManager::Instance().getUserAgent();
}

RobloxWebPage::RobloxWebPage(QObject* parent) 
    : QWebEnginePage(parent)
{
	connect(&RobloxNetworkAccessManager::Instance(), SIGNAL(finished(QNetworkReply*)), this, SLOT(handleFinished(QNetworkReply*)));
}

void RobloxWebPage::handleFinished(QNetworkReply* reply)
{
	QString status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString();
	if (status.toInt() != 200 || !reply->hasRawHeader("refresh"))
		return;

	QString acceptHeader = reply->request().rawHeader("accept").data();
	if (!acceptHeader.startsWith("application"))
		return;

	QString url = QString(reply->rawHeader("refresh").data());
	this->load(QUrl(url.mid(url.indexOf("=")+1)));
}

bool RobloxWebPage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame)
{
    RBX::RobloxGoogleAnalytics::trackEvent("Web", "URLRequest", url.toString().toStdString().c_str());

	if (!isMainFrame) // This is a new window request, open in default browser
	{
		QDesktopServices::openUrl(url);
		return false; // Don't navigate in this page
	}
	else if (type == QWebEnginePage::NavigationTypeLinkClicked)
	{
		Q_EMIT linkClicked(url);
		return false; // Delegate the link
	}
	else
		return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

QString RobloxWebPage::getDefaultUserAgent() const
{
	return this->profile()->httpUserAgent();
}

// If we have an override for the file upload element on this page, use that instead 
// This allows us to inject files into file input tags <input type="file" /> on the page
QString RobloxWebPage::chooseFile(const QString& oldFile )
{
	if (m_overideUploadFile.isEmpty())
		return oldFile; // or QFileDialog::getOpenFileName or something
	else
		return m_overideUploadFile;
}

void RobloxWebPage::setUploadFile(QString selector, QString fileName)
{
	// TODO: implement for QtWebEngine
	// Store for uploading
	m_overideUploadFile = fileName; 
	
	// In QtWebEngine, use runJavaScript to set the value
	// QString js = QString("document.querySelector('%1').value = '%2';").arg(selector, fileName);
	// runJavaScript(js);
}


