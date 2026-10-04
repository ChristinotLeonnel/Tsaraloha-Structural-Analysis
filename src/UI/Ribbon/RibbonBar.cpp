#include "RibbonBar.h"
#include "RibbonTab.h"
#include "../Theme/ThemeManager.h"

#include <QVBoxLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QHBoxLayout>
#include <QFrame>
#include "RibbonButton.h"

namespace TSA::UI
{

RibbonBar::RibbonBar(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &RibbonBar::updateTheme);
}

void RibbonBar::setupUi()
{
    setFixedHeight(126);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabBarAutoHide(false);

    m_tabWidget->setStyleSheet(ThemeManager::instance().ribbonTabWidgetStyleSheet());

    layout->addWidget(m_tabWidget);
}

void RibbonBar::setQuickAccess(const std::vector<QAction*>& actions)
{
    auto* host = new QWidget(m_tabWidget);
    auto* row = new QHBoxLayout(host);
    row->setContentsMargins(6, 0, 6, 0);
    row->setSpacing(1);

    for (auto* act : actions)
    {
        if (!act) continue;
        auto* btn = new RibbonButton(act, RibbonButtonSize::Small, host);
        btn->setIconOnly(true);
        row->addWidget(btn);
    }
    auto* sep = new QFrame(host);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setStyleSheet(ThemeManager::instance().ribbonSeparatorStyleSheet());
    row->addWidget(sep);

    m_tabWidget->setCornerWidget(host, Qt::TopLeftCorner);
}

void RibbonBar::updateTheme(bool /*isDark*/)
{
    if (m_tabWidget)
    {
        m_tabWidget->setStyleSheet(ThemeManager::instance().ribbonTabWidgetStyleSheet());
    }
}

RibbonTab* RibbonBar::addTab(const QString& title)
{
    auto* tab = new RibbonTab(m_tabWidget);
    m_tabWidget->addTab(tab, title);
    m_tabs.push_back(tab);
    return tab;
}

int RibbonBar::currentTabIndex() const
{
    return m_tabWidget ? m_tabWidget->currentIndex() : 0;
}

void RibbonBar::setCurrentTabIndex(int index)
{
    if (m_tabWidget && index >= 0 && index < m_tabWidget->count())
    {
        m_tabWidget->setCurrentIndex(index);
    }
}

} // namespace TSA::UI
