#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>

namespace TSA::UI
{

ThemeManager& ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager()
    : m_isDark(true)
{
}

void ThemeManager::setDarkMode(bool dark)
{
    if (m_isDark != dark)
    {
        m_isDark = dark;
        applyApplicationTheme();
        emit themeChanged(m_isDark);
    }
}

void ThemeManager::toggleTheme()
{
    setDarkMode(!m_isDark);
}

void ThemeManager::applyApplicationTheme()
{
    if (!qApp) return;

    if (m_isDark)
    {
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(0x1E, 0x23, 0x28));
        darkPalette.setColor(QPalette::WindowText, QColor(0xE6, 0xED, 0xF3));
        darkPalette.setColor(QPalette::Base, QColor(0x16, 0x1B, 0x22));
        darkPalette.setColor(QPalette::AlternateBase, QColor(0x21, 0x28, 0x30));
        darkPalette.setColor(QPalette::ToolTipBase, QColor(0x16, 0x1B, 0x22));
        darkPalette.setColor(QPalette::ToolTipText, QColor(0xF0, 0xF6, 0xFC));
        darkPalette.setColor(QPalette::Text, QColor(0xE6, 0xED, 0xF3));
        darkPalette.setColor(QPalette::Button, QColor(0x21, 0x28, 0x30));
        darkPalette.setColor(QPalette::ButtonText, QColor(0xE6, 0xED, 0xF3));
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(0x58, 0xA6, 0xFF));
        darkPalette.setColor(QPalette::Highlight, QColor(0x1F, 0x6F, 0xEB));
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6E, 0x76, 0x81));
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6E, 0x76, 0x81));
        qApp->setPalette(darkPalette);

        qApp->setStyleSheet(
            "QMainWindow { background-color: #1E2328; }"
            "QMenuBar {"
            "   background-color: #161B22;"
            "   color: #C9D1D9;"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 11px;"
            "   border-bottom: 1px solid #30363D;"
            "}"
            "QMenuBar::item {"
            "   background: transparent;"
            "   padding: 4px 10px;"
            "}"
            "QMenuBar::item:selected {"
            "   background-color: #2D333B;"
            "   color: #FFFFFF;"
            "   border-radius: 2px;"
            "}"
            "QMenu {"
            "   background-color: #1E2328;"
            "   color: #E6EDF3;"
            "   border: 1px solid #30363D;"
            "   padding: 3px;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 11px;"
            "}"
            "QMenu::item {"
            "   padding: 4px 20px 4px 24px;"
            "   border-radius: 2px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: #1F3A5A;"
            "   color: #58A6FF;"
            "}"
            "QMenu::separator {"
            "   height: 1px;"
            "   background-color: #30363D;"
            "   margin: 4px 6px;"
            "}"
            "QToolBar {"
            "   background-color: #161B22;"
            "   border: none;"
            "   margin: 0px;"
            "   padding: 0px;"
            "}"
            "QStatusBar {"
            "   background-color: #161B22;"
            "   color: #8B949E;"
            "   border-top: 1px solid #30363D;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 11px;"
            "}"
            "QDockWidget {"
            "   color: #E6EDF3;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 10px;"
            "   font-weight: bold;"
            "}"
            "QDockWidget::title {"
            "   background-color: #161B22;"
            "   padding: 4px 8px;"
            "   border-bottom: 1px solid #30363D;"
            "   border-top: 1px solid #30363D;"
            "   color: #C9D1D9;"
            "}"
            "QToolTip {"
            "   background-color: #161B22;"
            "   color: #F0F6FC;"
            "   border: 1px solid #484F58;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 11px;"
            "   padding: 3px 6px;"
            "   border-radius: 3px;"
            "}"
            "QSplitter::handle {"
            "   background-color: #161B22;"
            "}"
            "QTreeView, QTreeWidget {"
            "   background-color: #1E2328;"
            "   color: #E6EDF3;"
            "   border: 1px solid #30363D;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 11px;"
            "}"
            "QTreeView::item:selected, QTreeWidget::item:selected {"
            "   background-color: #1F3A5A;"
            "   color: #58A6FF;"
            "}"
            "QHeaderView::section {"
            "   background-color: #161B22;"
            "   color: #8B949E;"
            "   border: 1px solid #30363D;"
            "   padding: 3px 6px;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 10px;"
            "   font-weight: bold;"
            "}"
            "QScrollBar:vertical {"
            "   background: #161B22;"
            "   width: 8px;"
            "   margin: 0px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #30363D;"
            "   min-height: 20px;"
            "   border-radius: 3px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #484F58;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #161B22;"
            "   height: 8px;"
            "   margin: 0px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #30363D;"
            "   min-width: 20px;"
            "   border-radius: 3px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #484F58;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "}"
        );
    }
    else
    {
        // Light Theme (Style CAO Clair / AutoCAD Light)
        QPalette lightPalette;
        lightPalette.setColor(QPalette::Window, QColor(0xF6, 0xF8, 0xFA));
        lightPalette.setColor(QPalette::WindowText, QColor(0x24, 0x29, 0x2F));
        lightPalette.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
        lightPalette.setColor(QPalette::AlternateBase, QColor(0xF6, 0xF8, 0xFA));
        lightPalette.setColor(QPalette::ToolTipBase, QColor(0xFF, 0xFF, 0xFF));
        lightPalette.setColor(QPalette::ToolTipText, QColor(0x24, 0x29, 0x2F));
        lightPalette.setColor(QPalette::Text, QColor(0x24, 0x29, 0x2F));
        lightPalette.setColor(QPalette::Button, QColor(0xEB, 0xF0, 0xF4));
        lightPalette.setColor(QPalette::ButtonText, QColor(0x24, 0x29, 0x2F));
        lightPalette.setColor(QPalette::BrightText, Qt::red);
        lightPalette.setColor(QPalette::Link, QColor(0x09, 0x69, 0xDA));
        lightPalette.setColor(QPalette::Highlight, QColor(0x09, 0x69, 0xDA));
        lightPalette.setColor(QPalette::HighlightedText, Qt::white);
        lightPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x8C, 0x95, 0x9F));
        lightPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x8C, 0x95, 0x9F));
        qApp->setPalette(lightPalette);

        qApp->setStyleSheet(
            "QMainWindow { background-color: #F6F8FA; }"
            "QMenuBar {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 11px;"
            "   border-bottom: 1px solid #D0D7DE;"
            "}"
            "QMenuBar::item {"
            "   background: transparent;"
            "   padding: 4px 10px;"
            "}"
            "QMenuBar::item:selected {"
            "   background-color: #EAEEF2;"
            "   color: #0969DA;"
            "   border-radius: 2px;"
            "}"
            "QMenu {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   padding: 3px;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 11px;"
            "}"
            "QMenu::item {"
            "   padding: 4px 20px 4px 24px;"
            "   border-radius: 2px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: #DDF4FF;"
            "   color: #0969DA;"
            "}"
            "QMenu::separator {"
            "   height: 1px;"
            "   background-color: #D0D7DE;"
            "   margin: 4px 6px;"
            "}"
            "QToolBar {"
            "   background-color: #FFFFFF;"
            "   border: none;"
            "   margin: 0px;"
            "   padding: 0px;"
            "}"
            "QStatusBar {"
            "   background-color: #F6F8FA;"
            "   color: #57606A;"
            "   border-top: 1px solid #D0D7DE;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 11px;"
            "}"
            "QDockWidget {"
            "   color: #24292F;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 10px;"
            "   font-weight: bold;"
            "}"
            "QDockWidget::title {"
            "   background-color: #F6F8FA;"
            "   padding: 4px 8px;"
            "   border-bottom: 1px solid #D0D7DE;"
            "   border-top: 1px solid #D0D7DE;"
            "   color: #24292F;"
            "}"
            "QToolTip {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 11px;"
            "   padding: 3px 6px;"
            "   border-radius: 3px;"
            "}"
            "QSplitter::handle {"
            "   background-color: #D0D7DE;"
            "}"
            "QTreeView, QTreeWidget {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 11px;"
            "}"
            "QTreeView::item:selected, QTreeWidget::item:selected {"
            "   background-color: #DDF4FF;"
            "   color: #0969DA;"
            "}"
            "QHeaderView::section {"
            "   background-color: #F6F8FA;"
            "   color: #57606A;"
            "   border: 1px solid #D0D7DE;"
            "   padding: 3px 6px;"
            "   font-family: 'Segoe UI', sans-serif;"
            "   font-size: 10px;"
            "   font-weight: bold;"
            "}"
            "QScrollBar:vertical {"
            "   background: #F6F8FA;"
            "   width: 8px;"
            "   margin: 0px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #D0D7DE;"
            "   min-height: 20px;"
            "   border-radius: 3px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #AFB8C1;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "}"
            "QScrollBar:horizontal {"
            "   background: #F6F8FA;"
            "   height: 8px;"
            "   margin: 0px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #D0D7DE;"
            "   min-width: 20px;"
            "   border-radius: 3px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #AFB8C1;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "}"
        );
    }
}

QColor ThemeManager::rulerBackground() const
{
    return m_isDark ? QColor(0x16, 0x1B, 0x22) : QColor(0xF6, 0xF8, 0xFA);
}

QColor ThemeManager::rulerBorder() const
{
    return m_isDark ? QColor(0x30, 0x36, 0x3D) : QColor(0xD0, 0xD7, 0xDE);
}

QColor ThemeManager::rulerMajorTick() const
{
    return m_isDark ? QColor(0x6E, 0x76, 0x81) : QColor(0x6E, 0x77, 0x81);
}

QColor ThemeManager::rulerMinorTick() const
{
    return m_isDark ? QColor(0x38, 0x41, 0x4D) : QColor(0xAF, 0xB8, 0xC1);
}

QColor ThemeManager::rulerText() const
{
    return m_isDark ? QColor(0x8B, 0x94, 0x9E) : QColor(0x57, 0x60, 0x6A);
}

QString ThemeManager::ribbonTabWidgetStyleSheet() const
{
    if (m_isDark)
    {
        return
            "QTabWidget::pane {"
            "   border-top: 1px solid #30363D;"
            "   border-bottom: 1px solid #30363D;"
            "   background-color: #212830;"
            "}"
            "QTabBar {"
            "   background-color: #161B22;"
            "}"
            "QTabBar::tab {"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 11px;"
            "   font-weight: 600;"
            "   color: #8B949E;"
            "   background-color: #161B22;"
            "   border: 1px solid #282E37;"
            "   border-bottom: none;"
            "   padding: 4px 16px;"
            "   margin-right: 2px;"
            "   border-top-left-radius: 4px;"
            "   border-top-right-radius: 4px;"
            "}"
            "QTabBar::tab:hover {"
            "   background-color: #212830;"
            "   color: #E6EDF3;"
            "}"
            "QTabBar::tab:selected {"
            "   background-color: #212830;"
            "   color: #58A6FF;"
            "   border-color: #30363D;"
            "   border-top: 2px solid #1F6FEB;"
            "}";
    }
    else
    {
        return
            "QTabWidget::pane {"
            "   border-top: 1px solid #D0D7DE;"
            "   border-bottom: 1px solid #D0D7DE;"
            "   background-color: #FFFFFF;"
            "}"
            "QTabBar {"
            "   background-color: #F6F8FA;"
            "}"
            "QTabBar::tab {"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 11px;"
            "   font-weight: 600;"
            "   color: #57606A;"
            "   background-color: #F6F8FA;"
            "   border: 1px solid #D0D7DE;"
            "   border-bottom: none;"
            "   padding: 4px 16px;"
            "   margin-right: 2px;"
            "   border-top-left-radius: 4px;"
            "   border-top-right-radius: 4px;"
            "}"
            "QTabBar::tab:hover {"
            "   background-color: #FFFFFF;"
            "   color: #24292F;"
            "}"
            "QTabBar::tab:selected {"
            "   background-color: #FFFFFF;"
            "   color: #0969DA;"
            "   border-color: #D0D7DE;"
            "   border-top: 2px solid #0969DA;"
            "}";
    }
}

QString ThemeManager::ribbonScrollStyleSheet() const
{
    if (m_isDark)
    {
        return
            "QScrollArea {"
            "   background-color: #212830;"
            "   border: none;"
            "}"
            "QScrollBar:horizontal {"
            "   height: 4px;"
            "   background: #161B22;"
            "   margin: 0px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #30363D;"
            "   min-width: 20px;"
            "   border-radius: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #484F58;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "}";
    }
    else
    {
        return
            "QScrollArea {"
            "   background-color: #FFFFFF;"
            "   border: none;"
            "}"
            "QScrollBar:horizontal {"
            "   height: 4px;"
            "   background: #F6F8FA;"
            "   margin: 0px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "   background: #D0D7DE;"
            "   min-width: 20px;"
            "   border-radius: 2px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "   background: #AFB8C1;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "}";
    }
}

QString ThemeManager::ribbonContainerStyleSheet() const
{
    return m_isDark ? "background-color: #212830;" : "background-color: #FFFFFF;";
}

QString ThemeManager::ribbonPanelStyleSheet() const
{
    if (m_isDark)
    {
        return
            "TSA--UI--RibbonPanel {"
            "   background-color: transparent;"
            "   border-right: 1px solid #30363D;"
            "   margin-right: 2px;"
            "   padding-right: 4px;"
            "}";
    }
    else
    {
        return
            "TSA--UI--RibbonPanel {"
            "   background-color: transparent;"
            "   border-right: 1px solid #D0D7DE;"
            "   margin-right: 2px;"
            "   padding-right: 4px;"
            "}";
    }
}

QString ThemeManager::ribbonPanelTitleStyleSheet() const
{
    if (m_isDark)
    {
        return
            "QLabel {"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 9px;"
            "   font-weight: 600;"
            "   color: #8B949E;"
            "   background: transparent;"
            "   padding: 1px 2px;"
            "}"
            "QLabel:hover {"
            "   color: #E6EDF3;"
            "}";
    }
    else
    {
        return
            "QLabel {"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 9px;"
            "   font-weight: 600;"
            "   color: #57606A;"
            "   background: transparent;"
            "   padding: 1px 2px;"
            "}"
            "QLabel:hover {"
            "   color: #24292F;"
            "}";
    }
}

QString ThemeManager::ribbonSeparatorStyleSheet() const
{
    return m_isDark
        ? "QFrame { background-color: #30363D; border: none; margin: 4px 3px; }"
        : "QFrame { background-color: #D0D7DE; border: none; margin: 4px 3px; }";
}

QString ThemeManager::ribbonButtonLargeStyleSheet() const
{
    if (m_isDark)
    {
        return
            "QToolButton {"
            "   background-color: transparent;"
            "   border: 1px solid transparent;"
            "   border-radius: 3px;"
            "   padding: 1px 2px;"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 9.5px;"
            "   font-weight: 500;"
            "   color: #E6EDF3;"
            "}"
            "QToolButton:hover {"
            "   background-color: #2D333B;"
            "   border: 1px solid #444C56;"
            "   color: #FFFFFF;"
            "}"
            "QToolButton:pressed {"
            "   background-color: #373E47;"
            "   border: 1px solid #545D68;"
            "}"
            "QToolButton:checked {"
            "   background-color: #1F3A5A;"
            "   border: 1px solid #388BFD;"
            "   color: #58A6FF;"
            "   font-weight: 600;"
            "}"
            "QToolButton::menu-indicator {"
            "   subcontrol-origin: padding;"
            "   subcontrol-position: bottom right;"
            "   bottom: 1px;"
            "   right: 1px;"
            "}";
    }
    else
    {
        return
            "QToolButton {"
            "   background-color: transparent;"
            "   border: 1px solid transparent;"
            "   border-radius: 3px;"
            "   padding: 1px 2px;"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 9.5px;"
            "   font-weight: 500;"
            "   color: #24292F;"
            "}"
            "QToolButton:hover {"
            "   background-color: #EAEEF2;"
            "   border: 1px solid #D0D7DE;"
            "   color: #0969DA;"
            "}"
            "QToolButton:pressed {"
            "   background-color: #DDF4FF;"
            "   border: 1px solid #54AEFF;"
            "}"
            "QToolButton:checked {"
            "   background-color: #DDF4FF;"
            "   border: 1px solid #0969DA;"
            "   color: #0969DA;"
            "   font-weight: 600;"
            "}"
            "QToolButton::menu-indicator {"
            "   subcontrol-origin: padding;"
            "   subcontrol-position: bottom right;"
            "   bottom: 1px;"
            "   right: 1px;"
            "}";
    }
}

QString ThemeManager::ribbonButtonSmallStyleSheet() const
{
    if (m_isDark)
    {
        return
            "QToolButton {"
            "   background-color: transparent;"
            "   border: 1px solid transparent;"
            "   border-radius: 2px;"
            "   padding: 1px 5px;"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 10px;"
            "   color: #C9D1D9;"
            "   text-align: left;"
            "}"
            "QToolButton:hover {"
            "   background-color: #2D333B;"
            "   border: 1px solid #444C56;"
            "   color: #FFFFFF;"
            "}"
            "QToolButton:pressed {"
            "   background-color: #373E47;"
            "   border: 1px solid #545D68;"
            "}"
            "QToolButton:checked {"
            "   background-color: #1F3A5A;"
            "   border: 1px solid #388BFD;"
            "   color: #58A6FF;"
            "   font-weight: 600;"
            "}";
    }
    else
    {
        return
            "QToolButton {"
            "   background-color: transparent;"
            "   border: 1px solid transparent;"
            "   border-radius: 2px;"
            "   padding: 1px 5px;"
            "   font-family: 'Segoe UI', -apple-system, sans-serif;"
            "   font-size: 10px;"
            "   color: #24292F;"
            "   text-align: left;"
            "}"
            "QToolButton:hover {"
            "   background-color: #EAEEF2;"
            "   border: 1px solid #D0D7DE;"
            "   color: #0969DA;"
            "}"
            "QToolButton:pressed {"
            "   background-color: #DDF4FF;"
            "   border: 1px solid #54AEFF;"
            "}"
            "QToolButton:checked {"
            "   background-color: #DDF4FF;"
            "   border: 1px solid #0969DA;"
            "   color: #0969DA;"
            "   font-weight: 600;"
            "}";
    }
}

QString ThemeManager::topBarStyleSheet() const
{
    return m_isDark
        ? "background-color: #161B22; border-bottom: 1px solid #30363D;"
        : "background-color: #F6F8FA; border-bottom: 1px solid #D0D7DE;";
}

QString ThemeManager::topBarButtonStyleSheet() const
{
    if (m_isDark)
    {
        return
            "QToolButton {"
            "   background: #212830;"
            "   border: 1px solid #30363D;"
            "   border-radius: 2px;"
            "   color: #E6EDF3;"
            "   font-weight: bold;"
            "}"
            "QToolButton:hover { background: #30363D; border-color: #58A6FF; color: #58A6FF; }"
            "QToolButton:disabled { background: #161B22; border-color: #212830; color: #484F58; }";
    }
    else
    {
        return
            "QToolButton {"
            "   background: #FFFFFF;"
            "   border: 1px solid #D0D7DE;"
            "   border-radius: 2px;"
            "   color: #24292F;"
            "   font-weight: bold;"
            "}"
            "QToolButton:hover { background: #EAEEF2; border-color: #0969DA; color: #0969DA; }"
            "QToolButton:disabled { background: #F6F8FA; border-color: #D0D7DE; color: #8C959F; }";
    }
}

QString ThemeManager::topBarComboStyleSheet() const
{
    if (m_isDark)
    {
        return
            "QComboBox {"
            "   background: #161B22;"
            "   color: #E6EDF3;"
            "   border: 1px solid #30363D;"
            "   border-radius: 2px;"
            "   padding: 1px 6px;"
            "   font-size: 11px;"
            "   font-weight: 500;"
            "}"
            "QComboBox:hover { border-color: #58A6FF; }"
            "QComboBox::drop-down { border: none; width: 14px; }"
            "QComboBox QAbstractItemView {"
            "   background: #161B22;"
            "   color: #E6EDF3;"
            "   border: 1px solid #30363D;"
            "   selection-background-color: #1F3A5A;"
            "   selection-color: #58A6FF;"
            "}";
    }
    else
    {
        return
            "QComboBox {"
            "   background: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   border-radius: 2px;"
            "   padding: 1px 6px;"
            "   font-size: 11px;"
            "   font-weight: 500;"
            "}"
            "QComboBox:hover { border-color: #0969DA; }"
            "QComboBox::drop-down { border: none; width: 14px; }"
            "QComboBox QAbstractItemView {"
            "   background: #FFFFFF;"
            "   color: #24292F;"
            "   border: 1px solid #D0D7DE;"
            "   selection-background-color: #DDF4FF;"
            "   selection-color: #0969DA;"
            "}";
    }
}

} // namespace TSA::UI
