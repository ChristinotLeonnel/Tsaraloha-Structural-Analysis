#include "RibbonPanel.h"
#include "RibbonButton.h"
#include "../Theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QMenu>
#include <QAction>

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

    // Zone de contenu (boutons)
    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(2);
    m_contentLayout->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    mainLayout->addLayout(m_contentLayout, 1);

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

RibbonButton* RibbonPanel::addLargeAction(QAction* action, QMenu* menu)
{
    auto* btn = new RibbonButton(action, RibbonButtonSize::Large, this);
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
    std::vector<RibbonButton*> buttons;
    auto* colWidget = new QWidget(this);
    auto* colLayout = new QVBoxLayout(colWidget);
    colLayout->setContentsMargins(0, 0, 0, 0);
    colLayout->setSpacing(1);
    colLayout->setAlignment(Qt::AlignVCenter);

    for (auto* act : actions)
    {
        if (!act) continue;
        auto* btn = new RibbonButton(act, RibbonButtonSize::Small, colWidget);
        colLayout->addWidget(btn);
        buttons.push_back(btn);
    }

    m_contentLayout->addWidget(colWidget);
    return buttons;
}

RibbonButton* RibbonPanel::addMenuButton(const QString& text, const QIcon& icon, QMenu* menu, RibbonButtonSize size)
{
    auto* btn = new RibbonButton(text, icon, size, this);
    if (menu)
    {
        btn->setMenu(menu);
        btn->setPopupMode(QToolButton::InstantPopup);
    }
    m_contentLayout->addWidget(btn);
    return btn;
}

void RibbonPanel::addInternalSeparator()
{
    auto* sep = new QFrame(this);
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

} // namespace TSA::UI
