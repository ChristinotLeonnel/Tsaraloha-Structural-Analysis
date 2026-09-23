#include "RibbonButton.h"
#include "../Theme/ThemeManager.h"
#include <QAction>
#include <QMenu>

namespace TSA::UI
{

RibbonButton::RibbonButton(QAction* action, RibbonButtonSize size, QWidget* parent)
    : QToolButton(parent)
    , m_size(size)
{
    setDefaultAction(action);
    initStyle();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &RibbonButton::updateTheme);
}

RibbonButton::RibbonButton(const QString& text, const QIcon& icon, RibbonButtonSize size, QWidget* parent)
    : QToolButton(parent)
    , m_size(size)
{
    setText(text);
    setIcon(icon);
    initStyle();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &RibbonButton::updateTheme);
}

void RibbonButton::setRibbonSize(RibbonButtonSize size)
{
    m_size = size;
    initStyle();
}

void RibbonButton::updateTheme(bool /*isDark*/)
{
    initStyle();
}

void RibbonButton::initStyle()
{
    setAutoRaise(true);
    setFocusPolicy(Qt::NoFocus);

    if (m_size == RibbonButtonSize::Large)
    {
        setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setIconSize(QSize(28, 28));
        setFixedSize(54, 66);
        setStyleSheet(ThemeManager::instance().ribbonButtonLargeStyleSheet());
    }
    else // Small / Compact
    {
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        setIconSize(QSize(16, 16));
        setFixedHeight(21);
        setStyleSheet(ThemeManager::instance().ribbonButtonSmallStyleSheet());
    }
}

} // namespace TSA::UI
