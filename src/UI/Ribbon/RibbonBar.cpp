#include "RibbonBar.h"
#include "RibbonPanel.h"
#include "RibbonTab.h"
#include "../Theme/ThemeManager.h"

#include <QVBoxLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QHBoxLayout>
#include <QFrame>
#include "RibbonButton.h"

#include <algorithm>

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

int RibbonBar::tabCount() const
{
    return static_cast<int>(m_tabs.size());
}

QString RibbonBar::tabTitle(int index) const
{
    return m_tabWidget && index >= 0 && index < m_tabWidget->count() ? m_tabWidget->tabText(index) : QString();
}

RibbonTab* RibbonBar::tab(int index) const
{
    return index >= 0 && index < static_cast<int>(m_tabs.size()) ? m_tabs[index] : nullptr;
}

QHash<const QAction*, QString> RibbonBar::actionLocations() const
{
    QHash<const QAction*, QString> out;
    for (int i = 0; i < tabCount(); ++i)
        for (RibbonPanel* panel : m_tabs[i]->panels())
            for (QAction* a : panel->actions())
                if (!out.contains(a)) out.insert(a, tabTitle(i).remove(QLatin1Char('&')) + QStringLiteral(" › ") + panel->title());
    return out;
}

bool RibbonBar::revealAction(const QAction* action)
{
    for (int i = 0; i < tabCount(); ++i)
        for (RibbonPanel* panel : m_tabs[i]->panels())
        {
            const auto acts = panel->actions();
            if (std::find(acts.begin(), acts.end(), action) != acts.end())
            {
                setCurrentTabIndex(i);
                return true;
            }
        }
    return false;
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
