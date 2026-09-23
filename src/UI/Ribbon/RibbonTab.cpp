#include "RibbonTab.h"
#include "RibbonPanel.h"
#include "../Theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>

namespace TSA::UI
{

RibbonTab::RibbonTab(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &RibbonTab::updateTheme);
}

void RibbonTab::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFixedHeight(94);
    m_scrollArea->setStyleSheet(ThemeManager::instance().ribbonScrollStyleSheet());

    m_container = new QWidget(m_scrollArea);
    m_container->setStyleSheet(ThemeManager::instance().ribbonContainerStyleSheet());

    m_panelLayout = new QHBoxLayout(m_container);
    m_panelLayout->setContentsMargins(4, 1, 4, 1);
    m_panelLayout->setSpacing(2);
    m_panelLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_scrollArea->setWidget(m_container);
    mainLayout->addWidget(m_scrollArea);
}

void RibbonTab::updateTheme(bool /*isDark*/)
{
    if (m_scrollArea)
    {
        m_scrollArea->setStyleSheet(ThemeManager::instance().ribbonScrollStyleSheet());
    }
    if (m_container)
    {
        m_container->setStyleSheet(ThemeManager::instance().ribbonContainerStyleSheet());
    }
}

void RibbonTab::addPanel(RibbonPanel* panel)
{
    if (!panel) return;
    m_panels.push_back(panel);
    m_panelLayout->addWidget(panel);
}

} // namespace TSA::UI
