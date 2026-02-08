#pragma once

// RibbonButtonGroup.h
//
// A composite widget that groups small buttons (text-beside-icon style)
// arranged into a three-row grid (multiple columns as needed). Intended
// to be added to a SARibbonPanel via addWidget(...) for groups of small
// actions that should be laid out in a grid.
//
// Usage example (pseudo):
//   RibbonButtonGroup* group = new RibbonButtonGroup(panel);
//   group->addAction(copyAction);
//   group->addAction(cutAction);
//   group->addAction(duplicateAction);
//   panel->addWidget(group, SARibbonPanelItem::Small);
//
// Notes:
// - Uses SARibbonToolButton so it respects ribbon visuals already in repo.
// - Does not modify SARibbon sources.
// - Keeps simple resizing policy: buttons keep preferred sizes; layout recomputes
//   columns = ceil(count / 3).
//

#include <QWidget>
#include <QGridLayout>
#include <QList>
#include <QAction>
#include <QSize>

#include "SARibbonToolButton.h"

class RibbonButtonGroup : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonButtonGroup(QWidget* parent = nullptr);
    ~RibbonButtonGroup() override;

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    // Add an action — these are arranged in columns of up to 3 rows each.
    void addAction(QAction* act);

    // Clear all actions and buttons
    void clear();

    // Replace all actions (convenience)
    void setActions(const QList<QAction*>& actions);

    // Set icon size used for buttons (call before adding actions to affect them)
    void setIconSize(const QSize& s);

    // Getters
    QList<SARibbonToolButton*> buttons() const;

Q_SIGNALS:
    // Forwarded when any child button triggers
    void actionTriggered(QAction* action);

protected:
    void resizeEvent(QResizeEvent* ev) override;

private Q_SLOTS:
    void onChildTriggered();

private:
    // Helper to create and connect SARibbonToolButton for a given action
    SARibbonToolButton* createButtonForAction(QAction* action);

    // Core layout logic:
    // - buttons fill columns left-to-right, each column up to 3 rows
    void rebuildLayout();

private:
    QGridLayout* m_layout;
    QList<SARibbonToolButton*> m_buttons;
    QSize m_iconSize;
};