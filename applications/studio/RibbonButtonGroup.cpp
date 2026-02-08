#include "RibbonButtonGroup.h"
#include <QDebug>

RibbonButtonGroup::RibbonButtonGroup(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QGridLayout(this))
    , m_iconSize(16, 16)
{
    // Provide a small padding inside the container so buttons don't touch the group borders.
    m_layout->setContentsMargins(2, 2, 2, 2);
    // Small spacing between items so a tiny visible gap exists between buttons.
    m_layout->setSpacing(2);
    setLayout(m_layout);
    // Advertise preferred sizing and let the panel decide expansion.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // Small container stylesheet to avoid platform collapsing the outer inset.
    this->setStyleSheet("QWidget { padding: 4px; }");
}

RibbonButtonGroup::~RibbonButtonGroup()
{
    clear();
    // Layout and children are deleted by Qt parent/child system.
}

void RibbonButtonGroup::addAction(QAction* act)
{
    if (!act)
        return;

    SARibbonToolButton* btn = createButtonForAction(act);
    // Keep the "text beside icon" appearance requested by the XML
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIconSize(m_iconSize);

    m_buttons.append(btn);
    rebuildLayout();
}

void RibbonButtonGroup::clear()
{
    // Remove and delete wrappers (preferred) or buttons if no wrapper exists.
    for (SARibbonToolButton* b : qAsConst(m_buttons)) {
        if (!b)
            continue;
        disconnect(b, &SARibbonToolButton::triggered, this, &RibbonButtonGroup::onChildTriggered);

        QWidget* container = qobject_cast<QWidget*>(b->parentWidget());
        if (container && container->parent() == this) {
            // If we wrapped the button in a container, remove and delete the container.
            m_layout->removeWidget(container);
            container->deleteLater();
        } else {
            // Fallback: remove/delete the button itself.
            m_layout->removeWidget(b);
            b->deleteLater();
        }
    }
    m_buttons.clear();
    rebuildLayout();
}

void RibbonButtonGroup::setActions(const QList<QAction*>& actions)
{
    // clear existing
    for (SARibbonToolButton* b : qAsConst(m_buttons)) {
        disconnect(b, &SARibbonToolButton::triggered, this, &RibbonButtonGroup::onChildTriggered);
        m_layout->removeWidget(b);
        b->deleteLater();
    }
    m_buttons.clear();

    // add new
    for (QAction* act : actions) {
        if (act) {
            SARibbonToolButton* btn = createButtonForAction(act);
            // Keep the small/text-beside-icon presentation for grouped small actions
            btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            btn->setIconSize(m_iconSize);
            m_buttons.append(btn);
        }
    }
    rebuildLayout();
}

void RibbonButtonGroup::setIconSize(const QSize& s)
{
    m_iconSize = s;
    for (SARibbonToolButton* b : m_buttons)
        b->setIconSize(s);
}

QList<SARibbonToolButton*> RibbonButtonGroup::buttons() const
{
    return m_buttons;
}

SARibbonToolButton* RibbonButtonGroup::createButtonForAction(QAction* action)
{
    // Create a small container widget that will hold the button. This provides
    // per-item padding and ensures the group has visible gaps between elements.
    QWidget* wrapper = new QWidget(this);
    // Ensure the wrapper participates correctly in layout calculations for SARibbon
    // and does not steal focus. This helps the button receive events reliably.
    wrapper->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    wrapper->setFocusPolicy(Qt::NoFocus);
    wrapper->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    QHBoxLayout* wrapLayout = new QHBoxLayout(wrapper);
    wrapLayout->setContentsMargins(4, 2, 4, 2);
    wrapLayout->setSpacing(0);
    wrapper->setLayout(wrapLayout);

    // Create the actual tool button as a child of the wrapper so we can add/remove
    // the wrapper to/from the grid layout as a single unit.
    SARibbonToolButton* btn = new SARibbonToolButton(action, wrapper);
    // Use SmallButton so the button paints with text beside icon (consistent with XML "textbesideicon")
    btn->setButtonType(SARibbonToolButton::SmallButton);
    btn->setFocusPolicy(Qt::NoFocus);
    // Use a smaller stylesheet padding for a subtler look.
    btn->setStyleSheet("QToolButton { padding: 2px 6px; }");
    // Slightly reduce the minimum width so labels can still breathe but items are tighter.
    btn->setMinimumWidth(56);
    // Use a sane size policy so buttons size to their contents but allow the panel to lay them out.
    btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // Add the button into its wrapper layout.
    wrapLayout->addWidget(btn);

    // Ensure initial enabled state follows the QAction
    bool isEnabled = action ? action->isEnabled() : true;
    qDebug() << "RibbonButtonGroup: action created:" << (action ? action->objectName() : QString("<null>")) << "enabled=" << isEnabled;
    wrapper->setEnabled(isEnabled);
    btn->setEnabled(isEnabled);

    // Keep wrapper/button enabled state in sync with the QAction.
    // Use the wrapper as the connection context so the connection is
    // automatically disconnected if the wrapper is destroyed.
    if (action) {
        QObject::connect(action, &QAction::changed, wrapper, [action, wrapper, btn]() {
            bool en = action->isEnabled();
            if (wrapper)
                wrapper->setEnabled(en);
            if (btn)
                btn->setEnabled(en);
        });
    }

    connect(btn, &SARibbonToolButton::triggered, this, &RibbonButtonGroup::onChildTriggered);
    return btn;
}

void RibbonButtonGroup::rebuildLayout()
{
    // Remove all items from the layout (we will re-add widgets in the requested positions).
    QLayoutItem* it = nullptr;
    while ((it = m_layout->takeAt(0)) != nullptr) {
        delete it;
    }

    // If no widgets, update geometry and return.
    if (m_buttons.isEmpty()) {
        updateGeometry();
        return;
    }

    const int maxRows = 3;
    int count = m_buttons.count();

    // Place buttons in columns of up to 3 rows each.
    // Align left so natural gaps from spacing and margins are preserved.
    int maxCol = -1;
    for (int i = 0; i < count; ++i) {
        int col = i / maxRows;
        int row = i % maxRows;
        SARibbonToolButton* btn = m_buttons.at(i);
        // Prefer adding the wrapper container if present so per-item padding is preserved.
        QWidget* container = qobject_cast<QWidget*>(btn->parentWidget());
        if (container && container->parent() == this) {
            m_layout->addWidget(container, row, col, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
        } else {
            m_layout->addWidget(btn, row, col, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (col > maxCol) maxCol = col;
    }

    // Remove forced column/row stretching so columns size to their contents and gaps remain.
    // Let SARibbonPanelLayout allocate additional space when available.

    m_layout->invalidate();
    updateGeometry();
}

void RibbonButtonGroup::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    // Keep layout consistent; nothing heavy here
    rebuildLayout();
}

void RibbonButtonGroup::onChildTriggered()
{
    SARibbonToolButton* btn = qobject_cast<SARibbonToolButton*>(sender());
    if (!btn)
        return;
    QAction* a = btn->defaultAction();
    if (a)
        Q_EMIT actionTriggered(a);
}

QSize RibbonButtonGroup::sizeHint() const
{
    // Compute a size hint based on the children so SARibbonPanelLayout can
    // allocate an appropriate width. We arrange up to 3 rows per column.
    const int maxRows = 3;
    int count = m_buttons.count();
    if (count == 0)
        return QWidget::sizeHint();

    int cols = (count + maxRows - 1) / maxRows;
    int spacing = (m_layout) ? m_layout->spacing() : 0;
    QMargins margins = (m_layout) ? m_layout->contentsMargins() : QMargins();

    int totalW = margins.left() + margins.right();
    int maxButtonHeight = 0;

    for (int c = 0; c < cols; ++c) {
        int colW = 0;
        for (int r = 0; r < maxRows; ++r) {
            int idx = c * maxRows + r;
            if (idx >= count)
                break;
            SARibbonToolButton* b = m_buttons.at(idx);
            if (!b)
                continue;

            QSize bh;
            QWidget* container = qobject_cast<QWidget*>(b->parentWidget());
            if (container && container->parent() == this) {
                bh = container->sizeHint();
            } else {
                bh = b->sizeHint();
            }

            colW = qMax(colW, bh.width());
            maxButtonHeight = qMax(maxButtonHeight, bh.height());
        }
        totalW += colW;
        if (c != cols - 1)
            totalW += spacing;
    }

    int totalH = margins.top() + margins.bottom();
    // Height should account for up to maxRows button heights (stacked) plus spacing
    totalH += maxRows * maxButtonHeight;
    if (maxRows > 1)
        totalH += (maxRows - 1) * spacing;

    return QSize(totalW, totalH);
}

QSize RibbonButtonGroup::minimumSizeHint() const
{
    // Compute a conservative minimum size based on child minimumSizeHints.
    const int maxRows = 3;
    int count = m_buttons.count();
    if (count == 0)
        return QWidget::minimumSizeHint();

    int cols = (count + maxRows - 1) / maxRows;
    int spacing = (m_layout) ? m_layout->spacing() : 0;
    QMargins margins = (m_layout) ? m_layout->contentsMargins() : QMargins();

    int totalW = margins.left() + margins.right();
    int maxButtonMinHeight = 0;

    for (int c = 0; c < cols; ++c) {
        int colW = 0;
        for (int r = 0; r < maxRows; ++r) {
            int idx = c * maxRows + r;
            if (idx >= count)
                break;
            SARibbonToolButton* b = m_buttons.at(idx);
            if (!b)
                continue;

            QSize bh;
            QWidget* container = qobject_cast<QWidget*>(b->parentWidget());
            if (container && container->parent() == this) {
                bh = container->minimumSizeHint();
            } else {
                bh = b->minimumSizeHint();
            }

            colW = qMax(colW, bh.width());
            maxButtonMinHeight = qMax(maxButtonMinHeight, bh.height());
        }
        totalW += colW;
        if (c != cols - 1)
            totalW += spacing;
    }

    int totalH = margins.top() + margins.bottom();
    totalH += maxRows * maxButtonMinHeight;
    if (maxRows > 1)
        totalH += (maxRows - 1) * spacing;

    return QSize(totalW, totalH);
}
