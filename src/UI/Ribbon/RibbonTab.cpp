#include "RibbonTab.h"
#include "RibbonPanel.h"
#include "RibbonButton.h"
#include "../Theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QMenu>
#include <QResizeEvent>
#include <QIcon>
#include <QTimer>

namespace TSA::UI
{

namespace
{
constexpr int kMargin = 4;
}

RibbonTab::RibbonTab(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &RibbonTab::updateTheme);
}

void RibbonTab::setupUi()
{
    setFixedHeight(94);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_container = new QWidget(this);
    m_container->setStyleSheet(ThemeManager::instance().ribbonContainerStyleSheet());

    m_panelLayout = new QHBoxLayout(m_container);
    m_panelLayout->setContentsMargins(kMargin, 1, kMargin, 1);
    m_panelLayout->setSpacing(2);
    m_panelLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_moreButton = new RibbonButton(QObject::tr("Plus"), QIcon(), RibbonButtonSize::Large, m_container);
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    m_moreButton->setToolTip(QObject::tr("Autres commandes de cet onglet"));
    m_moreButton->setVisible(false);
    m_panelLayout->addWidget(m_moreButton);

    mainLayout->addWidget(m_container);
}

void RibbonTab::updateTheme(bool /*isDark*/)
{
    if (m_container)
    {
        m_container->setStyleSheet(ThemeManager::instance().ribbonContainerStyleSheet());
    }
    // Après les autres réceptions de themeChanged (boutons, panneaux) : nouvelles largeurs.
    for (auto* p : m_panels) p->invalidateWidths();
    QTimer::singleShot(0, this, [this] {
        if (isVisible()) relayout(availableWidth());
    });
}

void RibbonTab::addPanel(RibbonPanel* panel)
{
    if (!panel) return;
    m_panels.push_back(panel);
    m_panelLayout->insertWidget(static_cast<int>(m_panels.size()) - 1, panel);
}

QSize RibbonTab::sizeHint() const
{
    return QSize(600, 94);
}

QSize RibbonTab::minimumSizeHint() const
{
    // Le ruban se réorganise : il ne doit jamais imposer sa largeur à la fenêtre.
    return QSize(200, 94);
}

int RibbonTab::availableWidth() const
{
    // Un onglet masqué garde la largeur qu'il avait à son dernier affichage (la pile d'onglets ne
    // redimensionne que la page courante) : à l'affichage, la largeur de la pile fait foi.
    if (const QWidget* stack = parentWidget(); stack && stack->width() > 0)
        return stack->contentsRect().width();
    return width();
}

void RibbonTab::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (event->size().width() != m_lastWidth) relayout(event->size().width());
}

void RibbonTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    const int w = availableWidth();
    if (w != m_lastWidth) relayout(w);
    if (!m_verified)
    {
        m_verified = true;
        QTimer::singleShot(0, this, &RibbonTab::verifyMeasurements);
    }
}

void RibbonTab::verifyMeasurements()
{
    std::vector<int> before;
    for (auto* p : m_panels)
        for (auto mode : { RibbonPanelMode::Full, RibbonPanelMode::IconOnly, RibbonPanelMode::Collapsed })
            before.push_back(p->widthForMode(mode));
    for (auto* p : m_panels) p->invalidateWidths();
    std::vector<int> after;
    for (auto* p : m_panels)
        for (auto mode : { RibbonPanelMode::Full, RibbonPanelMode::IconOnly, RibbonPanelMode::Collapsed })
            after.push_back(p->widthForMode(mode));
    if (after != before && isVisible()) relayout(availableWidth());
}

void RibbonTab::relayout(int width)
{
    if (m_inRelayout || m_panels.empty()) return;
    m_inRelayout = true;
    m_lastWidth = width;
    ensurePolished();   // styles appliqués avant toute mesure (sans effet s'ils le sont déjà)

    const int spacing = m_panelLayout->spacing();
    const int available = width - 2 * kMargin;

    // Réinitialise : tout visible, mode complet.
    for (auto* p : m_panels)
    {
        p->setVisible(true);
        p->setMode(RibbonPanelMode::Full);
    }
    m_moreButton->setVisible(false);

    auto totalWidth = [&](const std::vector<int>& w)
    {
        int sum = 0, n = 0;
        for (int v : w) { if (v > 0) { sum += v; ++n; } }
        return sum + (n > 1 ? (n - 1) * spacing : 0);
    };

    const size_t n = m_panels.size();
    std::vector<int> widths(n);
    for (size_t i = 0; i < n; ++i) widths[i] = m_panels[i]->widthForMode(RibbonPanelMode::Full);

    std::vector<RibbonPanelMode> modes(n, RibbonPanelMode::Full);

    // Étape 2 : icônes seules pour tous les panneaux qui ont de petits boutons.
    if (totalWidth(widths) > available)
    {
        for (size_t i = 0; i < n; ++i)
        {
            if (!m_panels[i]->hasCompactableButtons()) continue;
            modes[i] = RibbonPanelMode::IconOnly;
            widths[i] = m_panels[i]->widthForMode(RibbonPanelMode::IconOnly);
        }
    }

    // Étape 3 : repli des panneaux, de droite à gauche, jusqu'à ce que tout tienne.
    for (size_t k = n; k-- > 0 && totalWidth(widths) > available;)
    {
        modes[k] = RibbonPanelMode::Collapsed;
        widths[k] = m_panels[k]->widthForMode(RibbonPanelMode::Collapsed);
    }

    // Étape 4 : si même replié tout est trop large, les derniers panneaux vont dans « Plus ».
    std::vector<RibbonPanel*> overflow;
    const int moreWidth = m_moreButton->sizeHint().width() + spacing;
    if (totalWidth(widths) > available)
    {
        while (totalWidth(widths) + moreWidth > available && n > overflow.size() + 1)
        {
            const size_t last = n - overflow.size() - 1;
            overflow.push_back(m_panels[last]);
            widths[last] = 0;
        }
    }

    for (size_t i = 0; i < n; ++i) m_panels[i]->setMode(modes[i]);
    for (auto* p : overflow) p->setVisible(false);

    if (!overflow.empty())
    {
        auto* menu = new QMenu(m_moreButton);
        for (auto it = overflow.rbegin(); it != overflow.rend(); ++it)
        {
            if ((*it)->isEmptyPanel()) continue;
            menu->addMenu((*it)->buildOverflowMenu(menu));
        }
        if (auto* old = m_moreButton->menu()) old->deleteLater();
        m_moreButton->setMenu(menu);
        m_moreButton->setVisible(true);
    }

    m_inRelayout = false;
}

} // namespace TSA::UI
