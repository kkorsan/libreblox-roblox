/**
 * RobloxWebPage.h	virtual QString chooseFile(const QString& oldFile);
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#pragma once

#include <QtWebEngineWidgets>

class RobloxWebPage : public QWebEnginePage
{
	Q_OBJECT

Q_SIGNALS:
	void linkClicked(const QUrl&);

public:

	RobloxWebPage(QObject* parent = nullptr);
	QString userAgentForUrl(const QUrl &url ) const;

	QString getDefaultUserAgent() const; // To get access to protected default user agent
	void setUploadFile(QString selector, QString fileName);

protected:
	virtual QString chooseFile(const QString& oldFile);
	virtual bool acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame);

private Q_SLOTS:

	void handleFinished(QNetworkReply*);
private:
	QString m_overideUploadFile;
	QPoint  m_contextPos;
};
