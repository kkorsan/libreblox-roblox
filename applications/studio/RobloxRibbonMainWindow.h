/**
 * RibbonMainWindow.h
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#pragma once

#include <QToolBar>
#include <QSize>

#include "SARibbonMainWindow.h"
#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonPanel.h"
#include "SARibbonGallery.h"
#include "SARibbonQuickAccessBar.h"

#include "ui_RBXMainWindow.h"

class RobloxMainWindow;
class QDomElement;
class QRadioButton;
class QCheckBox;
class QComboBox;
class QuickAccessConfigDialog;

class RobloxRibbonMainWindow : public SARibbonMainWindow, public Ui::RBXMainWindow
{
	Q_OBJECT

public:
	RobloxRibbonMainWindow(RobloxMainWindow* pMainWindow);

	void setupRibbonBar();

	SARibbonCategory*  createRibbonPage(const QString& objectName, const QString& pageTitle, int indexPosition = -1);
	SARibbonPanel* createRibbonGroup(SARibbonCategory* pParent, const QString& objectName, const QString& groupTitle = QString());

	static QString getDefaultSavePath();

protected:
	QStringList getRecentFiles() const;

	void updateInternalWidgetsState(QAction* pAction, bool enabledState, bool checkedState);

	void setCurrentDirectory(const QString& path);
	void updateRecentFile(const QString& fileName);

	void actionEvent(QActionEvent* event);

	// pure virtual function, to ver overridden in derived class
	virtual void updateRecentFilesUI() = 0;

	QString					m_LastDirectory;
	static const int        MAX_RECENT_FILES = 6; //the app will only remember the most 6 recent opened files

public Q_SLOTS:
    void toggleRibbonMinimize();

protected Q_SLOTS:
	// pure virtual slots to be overridden in derived class
	virtual bool openRecentFile(const QString& fileName) = 0;
	virtual void commonSlot(bool isChecked) = 0;

private Q_SLOTS:
	void handleActionGroupSelected(QAction *pAction);
	void handleButtonGroupSelected(QAbstractButton* pButton);
	void handleMenuActionTrigger(QAction*);
	void handleBasicObjectsGalleryItemClicked(SARibbonGalleryItem* item);
	void handleItemSelected(const QString& selectedItem);
	void handleActionSelected(QAction* pAction);

	void handleCheckBoxClicked(bool state);
	void handleComboBoxActivated(int index);

	void handleToolButtonClicked();
	void handleColorPickMenu();

	void handleRibbonMinimizationChanged(SARibbonBar::RibbonMode mode);

	void customizeQuickAccess();
	void handleStyleChange(QAction* action);

	void showGroupOptionsMenu();

	void updateProxyAction();

private:
	void parseAndCreateQuickAccessBar(const QDomElement &docElement);
	void parseAndCreateSystemButton(const QDomElement &docElement);
	void parseAndCreateRibbonBar(const QDomElement &docElement);
	void parseAndCreateChildren(const QDomElement& tabPageElement, QObject* parent);

	// Helper creation functions
	SARibbonCategory*				parseAndCreateRibbonPage(const QDomElement& domElement);
	SARibbonPanel*	parseAndCreateRibbonGroup(const QDomElement& domElement, QObject* pParent);
    QWidget*                parseAndCreateColorPicker(const QDomElement& domElement, QObject* pParent);
	QMenu*					parseAndCreateMenuPopupButton(const QDomElement& domElement, QObject* pParent);
	QMenu*					parseAndCreateMenu(const QDomElement& domElement, QObject* pParent);
	SARibbonGallery*	parseAndCreateRibbonGallery(const QDomElement& domElement, QObject* pParent);
	SARibbonGalleryItem* parseAndCreateGalleryItem(const QDomElement& domElement, QObject* pParent);
	QActionGroup*           parseAndCreateActionGroup(const QDomElement& domElement, QObject* pParent);
	QButtonGroup*           parseAnCreateButtonGroup(const QDomElement& domElement, QObject* parent);
	QAction*                parseAndCreateSplitButton(const QDomElement& domElement, QObject* parent);
	QAction*                parseAndCreateSeparator(const QDomElement& domElement, QObject* parent);
	QRadioButton*			parseAndCreateRadioButton(const QDomElement& domElement, QObject* parent);
	QCheckBox*				parseAndCreateCheckBox(const QDomElement& domElement, QObject* parent, bool actionMode = true);
	QComboBox*              parseAndCreateComboBox(const QDomElement& domElement, QObject* pParent);
	QAction*				parseAndCreateRibbonAction(const QDomElement& actionElement, QObject* parent );
	QMenu*                  parseAndCreateGroupOptionsMenu(const QDomElement& domElement, QObject* parent);
	QMenu*                  parseAndCreateSubMenu(const QDomElement& domElement, QObject* parent);

	// Utility functions
	QString getName(const QDomElement& domElement) const;
	QString getText(const QDomElement& domElement) const;
	QString getToolTip(const QDomElement& domElement) const;
	QSize getSize(const QDomElement& domElement, const QSize& size = QSize(16, 16)) const;
	QIcon getIcon(const QDomElement& domElement, const QSize& size = QSize(16, 16)) const;
	Qt::ToolButtonStyle getStyle(const QDomElement& domElement) const;
    enum QToolButton::ToolButtonPopupMode getPopupStyle(const QDomElement& domElement) const;
	QAction* getAction(const QDomElement& actionElement);

	void addWidgetToRibbonGroup(SARibbonPanel* pRibbonGroup, QWidget* pWidget);
	void addButtonToButtonGroup(QButtonGroup* pButtonGroup, QAbstractButton* pWidget);
	void addActionToParent(QAction* pAction, QObject* pParent, const QDomElement& domElement);

	void initialize();
	void cleanupToolBars();

	void updateFonts(const QFont& font);
	void createThemeOptions();
	void setTheme(SARibbonTheme themeId);

	QAction* findChildAction(const QString& actionName) const;
	void populateNameValueStore(const QString& storeID, const QObject *pObject);

	QString getFullName(QObject* pObject);

	// Data members
	QMap<QAction*, QWidget*>   m_mCorrelaryActionMap;
	QMap<QString, QAction*>    m_mProxyActions;

	QFont					   m_ribbonFont;

	SARibbonTheme       m_ribbonTheme;
	QAction*                   m_pRibbonMinimizeAction;

	QuickAccessConfigDialog*   m_pQuickAccessConfigDialog;

	bool                       m_bInitialized;
};


// Proxy class for binding a QMenu to a ToolButton in order
// to drive its behavior and appearance.

class ToolButtonProxyMenu : public QMenu
{
    Q_OBJECT
public:
    ToolButtonProxyMenu(QString name, QWidget* parent)
        : QMenu(name, parent)
       , toolButton(NULL)
       , boundAction(NULL)
       , selectChangesText(false)
    {}

    QToolButton*    toolButton;
    QAction*        boundAction;
    QString         settingName;
    bool            selectChangesText;

public Q_SLOTS:
    void            onChanged();
};
