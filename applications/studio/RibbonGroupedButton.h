#pragma once

// RibbonGroupedButton.h
//
// A small composite widget that groups a single "main" large button
// (icon above, text under icon) with a grid of "side" buttons that
// use text-beside-icon style arranged into a three-row grid (multiple
// columns as needed). Intended to be added to a SARibbonPanel via
// addWidget(...) so we can implement the Qtitan/Qt SARibbon-like
// grouped layout without modifying SARibbon sources.
//
// Usage example (pseudo):
//   RibbonGroupedButton* grouped = new RibbonGroupedButton(panel);
//   grouped->addMainAction(mainAction);       // big icon-under-text button
//   grouped->addSideAction(copyAction);
//   grouped->addSideAction(cutAction);
//   grouped->addSideAction(duplicateAction);
//   panel->addWidget(grouped, SARibbonPanelItem::Large);
//
// Notes:
// - Uses SARibbonToolButton so it respects ribbon visuals already in repo.
// - Does not modify SARibbon sources.
// - Keeps simple resizing policy: side buttons keep preferred sizes; layout recomputes
//   columns = ceil(sideCount / 3).
//

#include <QWidget>
#include <QGridLayout>
#include <QList>
#include <QAction>
#include <QSize>

#include "SARibbonToolButton.h"

class RibbonGroupedButton : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonGroupedButton(QWidget* parent = nullptr);
    ~RibbonGroupedButton() override;

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    // Add the "main" action — rendered as a Large button with text under icon.
    // Only one main action is kept; subsequent calls replace it.
    void addMainAction(QAction* act);

    // Add a side action — these are arranged in columns of up to 3 rows each.
    void addSideAction(QAction* act);

    // Clear all actions and buttons
    void clear();

    // Replace all side actions (convenience)
    void setSideActions(const QList<QAction*>& actions);

    // Set icon sizes used for main and side buttons (call before adding actions to affect them)
    void setMainIconSize(const QSize& s);
    void setSideIconSize(const QSize& s);

    // Getters
    SARibbonToolButton* mainButton() const;
    QList<SARibbonToolButton*> sideButtons() const;

Q_SIGNALS:
    // Forwarded when any child button triggers
    void actionTriggered(QAction* action);

protected:
    void resizeEvent(QResizeEvent* ev) override;

private Q_SLOTS:
    void onChildTriggered();

private:
    // Helper to create and connect SARibbonToolButton for a given action
    SARibbonToolButton* createButtonForAction(QAction* action, SARibbonToolButton::RibbonButtonType type);

    // Core layout logic:
    // - main button goes at column 0, spanning rows 0..2
    // - side buttons start at column 1 and fill columns left-to-right, each column up to 3 rows
    void rebuildLayout();

private:
    QGridLayout* m_layout;
    SARibbonToolButton* m_mainButton;
    QList<SARibbonToolButton*> m_sideButtons;
    QSize m_iconSize;
    QSize m_sideIconSize;
};
