#include "RibbonButton.h"
#include "../Theme/ThemeManager.h"
#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QHelpEvent>
#include <QToolTip>
#include <QKeySequence>
#include <QRegularExpression>
#include <algorithm>
#include <cstdlib>

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

// Libellé sur deux lignes pour les grands boutons : « Dessiner Poutre » → « Dessiner / Poutre ».
static QString wrapLabel(const QString& text)
{
    if (text.length() <= 9 || text.contains(QLatin1Char('\n'))) return text;
    const int mid = text.length() / 2;
    int best = -1;
    for (int i = 0; i < text.length(); ++i)
        if (text[i] == ' ' && (best < 0 || std::abs(i - mid) < std::abs(best - mid))) best = i;
    if (best < 0) return text;
    return text.left(best) + QLatin1Char('\n') + text.mid(best + 1);
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

void RibbonButton::setIconOnly(bool iconOnly)
{
    if (m_iconOnly == iconOnly) return;
    m_iconOnly = iconOnly;
    initStyle();
}

// Infobulle d'ingénieur : nom de la commande en gras, description, puis raccourci.
QString RibbonButton::richToolTip() const
{
    QString name = defaultAction() ? defaultAction()->text() : text();
    name.remove('&');
    if (name.endsWith("...")) name.chop(3);
    name = name.trimmed();

    QString desc = defaultAction() ? defaultAction()->toolTip() : toolTip();
    QString shortcut = (defaultAction() && !defaultAction()->shortcut().isEmpty())
        ? defaultAction()->shortcut().toString(QKeySequence::NativeText)
        : QString();

    // Les infobulles existantes se terminent souvent par « (Ctrl+N) » : on l'extrait.
    static const QRegularExpression trailingKey(QStringLiteral(R"(\s*\(([^()]+)\)\s*(\.\.\.)?$)"));
    const auto m = trailingKey.match(desc);
    // Une parenthèse finale n'est un raccourci que si elle ressemble à une touche (« Ctrl+N », « F »).
    static const QRegularExpression keyLike(QStringLiteral(R"(^[\w+\-]{1,16}$)"));
    if (m.hasMatch() && keyLike.match(m.captured(1)).hasMatch())
    {
        if (shortcut.isEmpty()) shortcut = m.captured(1);
        desc.truncate(m.capturedStart());
    }
    desc = desc.trimmed();
    if (desc.compare(name, Qt::CaseInsensitive) == 0) desc.clear();

    QString html = QStringLiteral("<b>%1</b>").arg(name.toHtmlEscaped());
    if (!desc.isEmpty()) html += QStringLiteral("<br/>%1").arg(desc.toHtmlEscaped());
    if (!shortcut.isEmpty()) html += QStringLiteral("<br/><i>Raccourci : %1</i>").arg(shortcut.toHtmlEscaped());
    return html;
}

bool RibbonButton::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip)
    {
        auto* he = static_cast<QHelpEvent*>(e);
        QToolTip::showText(he->globalPos(), richToolTip(), this);
        return true;
    }
    return QToolButton::event(e);
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
        // Largeur dictée par le libellé (pas de coupure), hauteur fixe.
        setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setIconSize(QSize(28, 28));
        setMinimumWidth(50);
        setMaximumWidth(QWIDGETSIZE_MAX);
        setFixedHeight(66);
        if (auto* act = defaultAction()) setText(wrapLabel(act->text()));
        else setText(wrapLabel(text()));
        setStyleSheet(ThemeManager::instance().ribbonButtonLargeStyleSheet());
    }
    else // Small / Compact
    {
        setIconSize(QSize(16, 16));
        setFixedHeight(21);
        if (m_iconOnly)
        {
            setToolButtonStyle(Qt::ToolButtonIconOnly);
            setFixedWidth(26);
        }
        else
        {
            setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            setMinimumWidth(0);
            setMaximumWidth(QWIDGETSIZE_MAX);
        }
        setStyleSheet(ThemeManager::instance().ribbonButtonSmallStyleSheet());
    }
}

} // namespace TSA::UI
