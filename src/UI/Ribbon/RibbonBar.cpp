#include "RibbonBar.h"
#include "RibbonTab.h"
#include "../Theme/ThemeManager.h"

#include <QVBoxLayout>
#include <QTabWidget>
#include <QTabBar>

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
