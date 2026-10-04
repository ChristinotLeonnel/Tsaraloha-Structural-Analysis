#include "RibbonPanel.h"
#include "RibbonButton.h"
#include "../Theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QMenu>
#include <QAction>
#include <QIcon>

namespace TSA::UI
{

RibbonPanel::RibbonPanel(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
{
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &RibbonPanel::updateTheme);
}

void RibbonPanel::setupUi()
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(92);

    setStyleSheet(ThemeManager::instance().ribbonPanelStyleSheet());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 1);
    mainLayout->setSpacing(1);

    // Zone de contenu (boutons) : hébergée dans un widget pour pouvoir être masquée en mode replié.
    m_contentHost = new QWidget(this);
    m_contentLayout = new QHBoxLayout(m_contentHost);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(2);
    m_contentLayout->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    mainLayout->addWidget(m_contentHost, 1);

    // Titre en bas de panneau façon AutoCAD / Revit
    m_lblTitle = new QLabel(m_title + " ▾", this);
    m_lblTitle->setAlignment(Qt::AlignCenter);
    m_lblTitle->setStyleSheet(ThemeManager::instance().ribbonPanelTitleStyleSheet());
    mainLayout->addWidget(m_lblTitle);
}

void RibbonPanel::updateTheme(bool /*isDark*/)
{
    setStyleSheet(ThemeManager::instance().ribbonPanelStyleSheet());
    if (m_lblTitle)
    {
        m_lblTitle->setStyleSheet(ThemeManager::instance().ribbonPanelTitleStyleSheet());
    }
    for (auto* sep : m_separators)
    {
        if (sep)
        {
            sep->setStyleSheet(ThemeManager::instance().ribbonSeparatorStyleSheet());
        }
    }
}

void RibbonPanel::trackAction(QAction* action)
{
    if (action) m_entries.push_back({ action });
}

void RibbonPanel::trackSeparator()
{
    if (!m_entries.empty() && m_entries.back().action) m_entries.push_back({ nullptr });
}

RibbonButton* RibbonPanel::addLargeAction(QAction* action, QMenu* menu)
{
    trackAction(action);
    auto* btn = new RibbonButton(action, RibbonButtonSize::Large, m_contentHost);
    if (menu)
    {
        btn->setMenu(menu);
        btn->setPopupMode(QToolButton::MenuButtonPopup);
    }
    m_contentLayout->addWidget(btn);
    return btn;
}

std::vector<RibbonButton*> RibbonPanel::addSmallColumn(const std::vector<QAction*>& actions)
{
    // Au plus 3 petits boutons par colonne : au-delà, la colonne dépasse la hauteur du panneau.
    constexpr size_t kMaxPerColumn = 3;
    std::vector<RibbonButton*> buttons;

    QVBoxLayout* colLayout = nullptr;
    size_t inColumn = 0;
    for (auto* act : actions)
    {
        if (!act) continue;
        if (!colLayout || inColumn == kMaxPerColumn)
        {
            auto* colWidget = new QWidget(m_contentHost);
            colLayout = new QVBoxLayout(colWidget);
            colLayout->setContentsMargins(0, 0, 0, 0);
            colLayout->setSpacing(1);
            colLayout->setAlignment(Qt::AlignVCenter);
            m_contentLayout->addWidget(colWidget);
            inColumn = 0;
        }
        auto* btn = new RibbonButton(act, RibbonButtonSize::Small, m_contentHost);
        colLayout->addWidget(btn);
        buttons.push_back(btn);
        m_smallButtons.push_back(btn);
        trackAction(act);
        ++inColumn;
    }
    return buttons;
}

RibbonButton* RibbonPanel::addMenuButton(const QString& text, const QIcon& icon, QMenu* menu, RibbonButtonSize size)
{
    auto* btn = new RibbonButton(text, icon, size, m_contentHost);
    if (menu)
    {
        btn->setMenu(menu);
        btn->setPopupMode(QToolButton::InstantPopup);
        // Les commandes du sous-menu restent atteignables depuis le menu replié.
        trackSeparator();
        for (auto* a : menu->actions())
            if (!a->isSeparator() && !a->menu()) trackAction(a);
    }
    m_contentLayout->addWidget(btn);
    return btn;
}

void RibbonPanel::addInternalSeparator()
{
    trackSeparator();
    auto* sep = new QFrame(m_contentHost);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setFixedWidth(1);
    sep->setStyleSheet(ThemeManager::instance().ribbonSeparatorStyleSheet());
    m_contentLayout->addWidget(sep);
    m_separators.push_back(sep);
}

void RibbonPanel::addCustomWidget(QWidget* widget)
{
    if (widget)
    {
        m_contentLayout->addWidget(widget);
    }
}

void RibbonPanel::setMode(RibbonPanelMode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;

    const bool collapsed = (mode == RibbonPanelMode::Collapsed);
    m_contentHost->setVisible(!collapsed);
    m_lblTitle->setVisible(!collapsed);

    if (collapsed && !m_collapsedButton)
    {
        QIcon icon;
        for (const auto& e : m_entries)
            if (e.action && !e.action->icon().isNull()) { icon = e.action->icon(); break; }
        m_collapsedButton = new RibbonButton(m_title, icon, RibbonButtonSize::Large, this);
        m_collapsedButton->setMaximumWidth(96);
        m_collapsedButton->setPopupMode(QToolButton::InstantPopup);
        m_collapsedButton->setToolTip(m_title);
        m_collapsedButton->setMenu(buildOverflowMenu(m_collapsedButton));
        static_cast<QVBoxLayout*>(layout())->insertWidget(0, m_collapsedButton, 1);
    }
    if (m_collapsedButton) m_collapsedButton->setVisible(collapsed);

    const bool iconOnly = (mode == RibbonPanelMode::IconOnly);
    for (auto* b : m_smallButtons) b->setIconOnly(iconOnly);
    layout()->invalidate();
}

int RibbonPanel::widthForMode(RibbonPanelMode mode)
{
    const RibbonPanelMode previous = m_mode;
    setMode(mode);
    layout()->activate();
    const int w = sizeHint().width();
    setMode(previous);
    return w;
}

QMenu* RibbonPanel::buildOverflowMenu(QWidget* parent) const
{
    auto* menu = new QMenu(m_title, parent);
    for (const auto& e : m_entries)
    {
        if (e.action) menu->addAction(e.action);
        else if (!menu->actions().isEmpty() && !menu->actions().back()->isSeparator()) menu->addSeparator();
    }
    return menu;
}

} // namespace TSA::UI
