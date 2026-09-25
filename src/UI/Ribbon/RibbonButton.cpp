#include "RibbonButton.h"
#include "../Theme/ThemeManager.h"
#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <algorithm>

namespace TSA::UI
{

static QIcon generateFallbackIcon(const QString& rawText, bool isLarge)
{
    const int size = isLarge ? 28 : 16;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    QString clean = rawText;
    clean.remove('&');
    clean.remove(QRegularExpression("\\(.*\\)"));
    clean = clean.trimmed();

    QString initials;
    const QStringList words = clean.split(' ', Qt::SkipEmptyParts);
    if (words.size() >= 2)
    {
        initials = words[0].left(1).toUpper() + words[1].left(1).toUpper();
    }
    else if (!clean.isEmpty())
    {
        initials = clean.left(std::min<int>(2, clean.length())).toUpper();
    }
    else
    {
        initials = "•";
    }

    uint hash = 0;
    for (QChar c : clean)
    {
        hash = (hash * 33) + c.unicode();
    }
    const QColor bg = QColor(30, 41, 59);
    const QColor border = QColor(56, 189, 248);

    p.setPen(QPen(border, 1.2));
    p.setBrush(bg);
    p.drawRoundedRect(1, 1, size - 2, size - 2, 4, 4);

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(isLarge ? 11 : 9);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, initials);

    return QIcon(pix);
}

RibbonButton::RibbonButton(QAction* action, RibbonButtonSize size, QWidget* parent)
    : QToolButton(parent)
    , m_size(size)
{
    if (action && action->icon().isNull())
    {
        action->setIcon(generateFallbackIcon(action->text(), m_size == RibbonButtonSize::Large));
    }
    setDefaultAction(action);
    initStyle();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &RibbonButton::updateTheme);
}

RibbonButton::RibbonButton(const QString& text, const QIcon& icon, RibbonButtonSize size, QWidget* parent)
    : QToolButton(parent)
    , m_size(size)
{
    setText(text);
    if (icon.isNull())
    {
        setIcon(generateFallbackIcon(text, size == RibbonButtonSize::Large));
    }
    else
    {
        setIcon(icon);
    }
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
