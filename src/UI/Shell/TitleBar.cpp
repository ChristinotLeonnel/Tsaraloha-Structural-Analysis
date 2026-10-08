#include "TitleBar.h"
#include "App/ProductInfo.h"

#include "../Theme/ThemeManager.h"

#include <QAbstractButton>
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QToolButton>
#include <QWindow>

#include <algorithm>

namespace TSA::UI
{

namespace
{
constexpr int kHeight = 36;
constexpr int kWindowButtonWidth = 46;

// Glyphes « Segoe MDL2 Assets » (Windows 10+) : mêmes symboles que les fenêtres natives.
const QString kGlyphMinimize = QString(QChar(0xE921));
const QString kGlyphMaximize = QString(QChar(0xE922));
const QString kGlyphRestore = QString(QChar(0xE923));
const QString kGlyphClose = QString(QChar(0xE8BB));
} // namespace

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("TSATitleBar");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(kHeight);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    // Gauche : bouton d'application + accès rapide
    m_left = new QWidget(this);
    m_leftLayout = new QHBoxLayout(m_left);
    m_leftLayout->setContentsMargins(4, 0, 4, 0);
    m_leftLayout->setSpacing(1);

    m_appMenu = new QMenu(this);
    m_appButton = new QToolButton(m_left);
    m_appButton->setObjectName("TitleBarAppButton");
    m_appButton->setIcon(QIcon(QString::fromLatin1(TSA::Product::kIconSvg)));
    m_appButton->setIconSize(QSize(22, 22));
    m_appButton->setFixedSize(40, kHeight - 4);
    m_appButton->setPopupMode(QToolButton::InstantPopup);
    m_appButton->setMenu(m_appMenu);
    m_appButton->setToolTip(tr("Menu de l'application %1").arg(TSA::Product::name()));
    m_leftLayout->addWidget(m_appButton);
    addQuickAccessSeparator();
    row->addWidget(m_left);

    row->addStretch(1);

    // Droite : autres commandes puis boutons de fenêtre
    m_right = new QWidget(this);
    auto* rightLayout = new QHBoxLayout(m_right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    auto* trailingHost = new QWidget(m_right);
    m_trailingLayout = new QHBoxLayout(trailingHost);
    m_trailingLayout->setContentsMargins(4, 0, 8, 0);
    m_trailingLayout->setSpacing(1);
    rightLayout->addWidget(trailingHost);

    m_btnMinimize = makeWindowButton(kGlyphMinimize, "TitleBarMinimize", tr("Réduire"));
    m_btnMaximize = makeWindowButton(kGlyphMaximize, "TitleBarMaximize", tr("Agrandir"));
    m_btnClose = makeWindowButton(kGlyphClose, "TitleBarClose", tr("Fermer"));
    for (auto* b : { m_btnMinimize, m_btnMaximize, m_btnClose }) rightLayout->addWidget(b);
    row->addWidget(m_right);

    connect(m_btnMinimize, &QToolButton::clicked, this, [this] { window()->showMinimized(); });
    connect(m_btnMaximize, &QToolButton::clicked, this, [this] {
        window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
    });
    connect(m_btnClose, &QToolButton::clicked, this, [this] { window()->close(); });

    // Titre : hors mise en page, centré sur toute la largeur et jamais sous les boutons.
    m_title = new QLabel(this);
    m_title->setObjectName("TitleBarTitle");
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &TitleBar::applyTheme);
    applyTheme(ThemeManager::instance().isDarkMode());
}

QToolButton* TitleBar::makeCommandButton(QAction* action)
{
    auto* b = new QToolButton(this);
    b->setObjectName("TitleBarCommand");
    b->setDefaultAction(action);
    b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    b->setIconSize(QSize(18, 18));
    b->setFixedSize(30, kHeight - 6);
    b->setAutoRaise(true);
    // Le bouton suit la visibilité de l'action (une barre d'outils le ferait, pas un QToolButton seul).
    b->setVisible(action->isVisible());
    connect(action, &QAction::changed, b, [b, action] { b->setVisible(action->isVisible()); });
    return b;
}

QToolButton* TitleBar::addQuickAccess(QAction* action)
{
    auto* b = makeCommandButton(action);
    b->setParent(m_left);
    m_leftLayout->addWidget(b);
    placeTitle();
    return b;
}

void TitleBar::addQuickAccessSeparator()
{
    auto* sep = new QWidget(m_left);
    sep->setObjectName("TitleBarSeparator");
    sep->setAttribute(Qt::WA_StyledBackground, true);
    sep->setFixedSize(1, 18);
    m_leftLayout->addSpacing(5);
    m_leftLayout->addWidget(sep);
    m_leftLayout->addSpacing(5);
}

QToolButton* TitleBar::addTrailing(QAction* action)
{
    auto* b = makeCommandButton(action);
    b->setParent(m_trailingLayout->parentWidget());
    m_trailingLayout->addWidget(b);
    placeTitle();
    return b;
}

QToolButton* TitleBar::makeWindowButton(const QString& glyph, const QString& objectName, const QString& toolTip)
{
    auto* b = new QToolButton(this);
    b->setObjectName(objectName);
    b->setText(glyph);
    b->setToolTip(toolTip);
    b->setFixedSize(kWindowButtonWidth, kHeight);
    b->setFocusPolicy(Qt::NoFocus);
    QFont f(QStringLiteral("Segoe MDL2 Assets"));
    f.setPointSizeF(7.5);
    b->setFont(f);
    return b;
}

void TitleBar::setTitle(const QString& title)
{
    m_fullTitle = title;
    placeTitle();
}

void TitleBar::placeTitle()
{
    if (!m_title) return;
    // Zone libre entre les groupes gauche et droit ; le titre est centré sur la barre si possible.
    const int margin = 12;
    const int freeLeft = m_left->geometry().right() + margin;
    const int freeRight = m_right->geometry().left() - margin;
    const int available = std::max(0, freeRight - freeLeft);
    const QString text = m_title->fontMetrics().elidedText(m_fullTitle, Qt::ElideMiddle, available);
    m_title->setText(text);
    const int w = std::min(available, m_title->fontMetrics().horizontalAdvance(text) + 4);
    int x = (width() - w) / 2;
    x = std::clamp(x, freeLeft, std::max(freeLeft, freeRight - w));
    m_title->setGeometry(x, 0, w, height());
    m_title->setVisible(available > 40);
}

bool TitleBar::isCaptionArea(const QPoint& localPos) const
{
    if (!rect().contains(localPos)) return false;
    const QWidget* child = childAt(localPos);
    for (const QWidget* w = child; w && w != this; w = w->parentWidget())
        if (qobject_cast<const QAbstractButton*>(w)) return false;
    return true;
}

void TitleBar::updateWindowState(Qt::WindowStates state)
{
    const bool maximized = state.testFlag(Qt::WindowMaximized);
    m_btnMaximize->setText(maximized ? kGlyphRestore : kGlyphMaximize);
    m_btnMaximize->setToolTip(maximized ? tr("Restaurer") : tr("Agrandir"));
    const bool fullScreen = state.testFlag(Qt::WindowFullScreen);
    m_btnMaximize->setEnabled(!fullScreen);
}

void TitleBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    placeTitle();
}

// Repli hors Windows (ou si la fenêtre n'intercepte pas WM_NCHITTEST) : déplacement et
// double-clic gérés par Qt. Sous Windows, la zone de titre est non cliente : ces événements n'arrivent pas.
void TitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isCaptionArea(event->position().toPoint()) && window()->windowHandle())
    {
        window()->windowHandle()->startSystemMove();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isCaptionArea(event->position().toPoint()))
    {
        window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TitleBar::applyTheme(bool dark)
{
    const QString bg = dark ? "#161B22" : "#F6F8FA";
    const QString border = dark ? "#30363D" : "#D0D7DE";
    const QString text = dark ? "#C9D1D9" : "#24292F";
    const QString hover = dark ? "#2D333B" : "#EAEEF2";
    const QString pressed = dark ? "#373E47" : "#DDE3E9";
    setStyleSheet(QStringLiteral(
        "#TSATitleBar { background: %1; border-bottom: 1px solid %2; }"
        "#TitleBarTitle { color: %3; font-family: 'Segoe UI'; font-size: 12px; }"
        "QToolButton#TitleBarAppButton, QToolButton#TitleBarCommand { background: transparent; border: none; border-radius: 3px; }"
        "QToolButton#TitleBarAppButton::menu-indicator { image: none; width: 0; }"
        "QToolButton#TitleBarAppButton:hover, QToolButton#TitleBarCommand:hover { background: %4; }"
        "QToolButton#TitleBarAppButton:pressed, QToolButton#TitleBarCommand:pressed { background: %5; }"
        "QToolButton#TitleBarCommand:disabled { background: transparent; }"
        "#TitleBarSeparator { background: %2; }"
        "QToolButton#TitleBarMinimize, QToolButton#TitleBarMaximize, QToolButton#TitleBarClose {"
        "   background: transparent; border: none; color: %3; }"
        "QToolButton#TitleBarMinimize:hover, QToolButton#TitleBarMaximize:hover { background: %4; }"
        "QToolButton#TitleBarMinimize:pressed, QToolButton#TitleBarMaximize:pressed { background: %5; }"
        "QToolButton#TitleBarMaximize:disabled { color: %2; }"
        "QToolButton#TitleBarClose:hover { background: #E81123; color: white; }"
        "QToolButton#TitleBarClose:pressed { background: #F1707A; color: white; }")
        .arg(bg, border, text, hover, pressed));
}

} // namespace TSA::UI
