#include "RibbonGroupedButton.h"

#include <QLayoutItem>
#include <QResizeEvent>
#include <QStyle>
#include <QDebug>

RibbonGroupedButton::RibbonGroupedButton(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QGridLayout(this))
    , m_mainButton(nullptr)
    , m_iconSize(32, 32)
    , m_sideIconSize(16, 16)
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);
    setLayout(m_layout);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

RibbonGroupedButton::~RibbonGroupedButton()
{
    clear();
    // Layout and children are deleted by Qt parent/child system.
}

void RibbonGroupedButton::addMainAction(QAction* act)
{
    if (!act)
        return;

    // Remove existing main button (if present)
    if (m_mainButton) {
        disconnect(m_mainButton, &SARibbonToolButton::triggered, this, &RibbonGroupedButton::onChildTriggered);
        m_layout->removeWidget(m_mainButton);
        m_mainButton->deleteLater();
        m_mainButton = nullptr;
    }

    // Create SARibbonToolButton and configure it
    SARibbonToolButton* btn = createButtonForAction(act, SARibbonToolButton::LargeButton);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setIconSize(m_iconSize);
    btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_mainButton = btn;

    rebuildLayout();
}

void RibbonGroupedButton::addSideAction(QAction* act)
{
    if (!act)
        return;

    SARibbonToolButton* btn = createButtonForAction(act, SARibbonToolButton::SmallButton);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIconSize(m_sideIconSize);
    btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_sideButtons.append(btn);

    rebuildLayout();
}

void RibbonGroupedButton::clear()
{
    if (m_mainButton) {
        disconnect(m_mainButton, &SARibbonToolButton::triggered, this, &RibbonGroupedButton::onChildTriggered);
        m_layout->removeWidget(m_mainButton);
        m_mainButton->deleteLater();
        m_mainButton = nullptr;
    }
    for (SARibbonToolButton* b : qAsConst(m_sideButtons)) {
        disconnect(b, &SARibbonToolButton::triggered, this, &RibbonGroupedButton::onChildTriggered);
        m_layout->removeWidget(b);
        b->deleteLater();
    }
    m_sideButtons.clear();

    // clear any remaining layout items
    QLayoutItem* it = nullptr;
    while ((it = m_layout->takeAt(0)) != nullptr) {
        delete it;
    }
}

void RibbonGroupedButton::setMainIconSize(const QSize& s)
{
    m_iconSize = s;
    if (m_mainButton)
        m_mainButton->setIconSize(s);
}

void RibbonGroupedButton::setSideIconSize(const QSize& s)
{
    m_sideIconSize = s;
    for (SARibbonToolButton* b : m_sideButtons)
        b->setIconSize(s);
}

void RibbonGroupedButton::setSideActions(const QList<QAction*>& actions)
{
    // clear existing
    for (SARibbonToolButton* b : qAsConst(m_sideButtons)) {
        disconnect(b, &SARibbonToolButton::triggered, this, &RibbonGroupedButton::onChildTriggered);
        m_layout->removeWidget(b);
        b->deleteLater();
    }
    m_sideButtons.clear();

    for (QAction* a : actions)
        addSideAction(a);
}

SARibbonToolButton* RibbonGroupedButton::mainButton() const
{
    return m_mainButton;
}

QList<SARibbonToolButton*> RibbonGroupedButton::sideButtons() const
{
    return m_sideButtons;
}

SARibbonToolButton* RibbonGroupedButton::createButtonForAction(QAction* action, SARibbonToolButton::RibbonButtonType type)
{
    SARibbonToolButton* btn = new SARibbonToolButton(action, this);
    btn->setButtonType(type);
    btn->setFocusPolicy(Qt::NoFocus);
    connect(btn, &SARibbonToolButton::triggered, this, &RibbonGroupedButton::onChildTriggered);
    return btn;
}

void RibbonGroupedButton::rebuildLayout()
{
    // Remove all items from the layout (we will re-add widgets in the requested positions).
    QLayoutItem* it = nullptr;
    while ((it = m_layout->takeAt(0)) != nullptr) {
        delete it;
    }

    // If no widgets at all, update geometry and return.
    if (!m_mainButton && m_sideButtons.isEmpty()) {
        updateGeometry();
        return;
    }

    const int maxRows = 3;
    int rowSpan = maxRows;

    // Place main button at column 0 spanning the rows if present
    if (m_mainButton) {
        m_layout->addWidget(m_mainButton, 0, 0, rowSpan, 1, Qt::AlignCenter);
    }

    // Place side buttons starting at column 1 (or 0 if no main)
    int sideCount = m_sideButtons.count();
    if (sideCount > 0) {
        int colBase = m_mainButton ? 1 : 0;
        for (int i = 0; i < sideCount; ++i) {
            int col = (i / maxRows) + colBase;
            int row = i % maxRows;
            SARibbonToolButton* btn = m_sideButtons.at(i);
            m_layout->addWidget(btn, row, col, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    m_layout->invalidate();
    updateGeometry();
}

void RibbonGroupedButton::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    // Keep layout consistent; nothing heavy here
    rebuildLayout();
}

void RibbonGroupedButton::onChildTriggered()
{
    SARibbonToolButton* btn = qobject_cast<SARibbonToolButton*>(sender());
    if (!btn)
        return;
    QAction* a = btn->defaultAction();
    if (a)
        Q_EMIT actionTriggered(a);
}

QSize RibbonGroupedButton::sizeHint() const
{
    return m_layout->sizeHint();
}

QSize RibbonGroupedButton::minimumSizeHint() const
{
    return m_layout->minimumSize();
}
