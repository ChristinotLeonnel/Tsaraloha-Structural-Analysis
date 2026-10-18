// Représentation du modèle dans la vue 3D (Affichage > Représentation) : physique, filaire analytique,
// éléments finis, superposition. Visuel seulement : rien n'est modifié dans le modèle ni dans les
// résultats. Logique de rendu : src/Viewer/OccView_DisplayMode.cpp ; politique : ModelDisplayMode.h.

#include "MainWindow.h"
#include "UI/Theme/ThemedIcon.h"

#include "Dock/LogConsoleDock.h"
#include "../Viewer/OccView.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QLabel>
#include <QSettings>

using TSA::Viewer::ModelDisplayMode;
using TSA::Viewer::RenderDisplayMode;

void MainWindow::createDisplayModeActions()
{
    m_displayModeGroup = new QActionGroup(this);
    m_displayModeGroup->setExclusive(true);
    struct Def
    {
        ModelDisplayMode mode;
        const char* icon;
        QString text;
        QString tip;
    };
    const Def defs[] = {
        { ModelDisplayMode::Physical, ":/icons/view/display_physical.svg", tr("Modèle\nphysique"),
          tr("Modèle physique : éléments avec leurs sections et leur géométrie habituelles") },
        { ModelDisplayMode::Analytical, ":/icons/view/display_analytical.svg", tr("Filaire\nanalytique"),
          tr("Modèle filaire analytique : sections masquées, axes des barres et nœuds visibles ; supports, charges, "
             "sélection et accrochage conservés. Affichage seulement : aucune donnée de calcul n'est modifiée.") },
        { ModelDisplayMode::FiniteElement, ":/icons/view/display_fe.svg", tr("Éléments finis"),
          tr("Modèle éléments finis : maillage réellement transmis au moteur lors du dernier calcul à jour "
             "(nœuds et éléments du solveur). Aucune subdivision n'est inventée.") },
        { ModelDisplayMode::Overlay, ":/icons/view/view_transparent.svg", tr("Superposition"),
          tr("Superposition : sections translucides et axes analytiques par-dessus") },
    };
    for (const Def& d : defs)
    {
        auto* a = new QAction(TSA::UI::themedIcon(d.icon), d.text, this);
        a->setCheckable(true);
        a->setToolTip(d.tip);
        a->setStatusTip(d.tip);
        a->setData(static_cast<int>(d.mode));
        a->setChecked(d.mode == ModelDisplayMode::Physical);
        m_displayModeGroup->addAction(a);
        m_displayModeActions.push_back(a);
    }
    connect(m_displayModeGroup, &QActionGroup::triggered, this, &MainWindow::onModelDisplayModeTriggered);
}

void MainWindow::onModelDisplayModeTriggered(QAction* action)
{
    if (!m_occView || !action) return;
    m_occView->setModelDisplayMode(static_cast<ModelDisplayMode>(action->data().toInt()));
    reportModelDisplayMode();
}

void MainWindow::reportModelDisplayMode()
{
    if (!m_occView) return;
    const ModelDisplayMode mode = m_occView->modelDisplayMode();
    QString text = tr("Représentation : %1").arg(QCoreApplication::translate("TSA::Viewer::ModelDisplayMode", TSA::Viewer::modelDisplayModeName(mode)));
    if (mode == ModelDisplayMode::FiniteElement)
    {
        const auto status = m_occView->solverMeshStatus();
        text += QStringLiteral(" — ") + QString::fromStdString(status.message);
        if (m_consoleDock) m_consoleDock->appendLog(QString::fromStdString(status.message), status.available ? "INFO" : "WARN");
    }
    if (m_statusInfo) m_statusInfo->setText(text);
}

void MainWindow::createRenderModeActions()
{
    // Préférences d'affichage seulement : aucune donnée du modèle ni du calcul n'en dépend.
    QSettings settings;
    const int saved = settings.value("view/renderMode", static_cast<int>(RenderDisplayMode::Materials)).toInt();
    m_renderModeGroup = new QActionGroup(this);
    m_renderModeGroup->setExclusive(true);
    struct Def
    {
        RenderDisplayMode mode;
        const char* icon;
        QString text;
        QString tip;
    };
    const Def defs[] = {
        { RenderDisplayMode::Materials, ":/icons/view/view_3d.svg", tr("Texturé"),
          tr("Rendu texturé : finition d'aspect de chaque matériau (béton coffré, acier peint, câble…)") },
        { RenderDisplayMode::Structure, ":/icons/view/view_shaded.svg", tr("Ombré"), tr("Rendu ombré : couleur unie du matériau, sans texture") },
        { RenderDisplayMode::Wireframe, ":/icons/view/view_wireframe.svg", tr("Filaire"), tr("Rendu filaire : arêtes des volumes seulement") },
    };
    for (const Def& d : defs)
    {
        auto* a = new QAction(TSA::UI::themedIcon(d.icon), d.text, this);
        a->setCheckable(true);
        a->setToolTip(d.tip);
        a->setStatusTip(d.tip);
        a->setData(static_cast<int>(d.mode));
        a->setChecked(static_cast<int>(d.mode) == saved);
        m_renderModeGroup->addAction(a);
        m_renderModeActions.push_back(a);
    }
    if (!m_renderModeGroup->checkedAction()) m_renderModeActions.front()->setChecked(true);
    m_actionRenderEdges = new QAction(tr("Arêtes"), this);
    m_actionRenderEdges->setCheckable(true);
    m_actionRenderEdges->setChecked(settings.value("view/renderEdges", false).toBool());
    m_actionRenderEdges->setToolTip(tr("Dessiner les arêtes des faces sur les volumes ombrés ou texturés"));
    m_actionRenderLightweight = new QAction(tr("Mode léger"), this);
    m_actionRenderLightweight->setCheckable(true);
    m_actionRenderLightweight->setChecked(settings.value("view/renderLightweight", false).toBool());
    m_actionRenderLightweight->setToolTip(tr("Mode léger pour PC modestes : sans texture ni arête, facettisation 4 fois plus grossière. "
                                             "Affichage seulement : le calcul n'est pas modifié."));
    connect(m_renderModeGroup, &QActionGroup::triggered, this, [this] { applyRenderOptions(); });
    connect(m_actionRenderEdges, &QAction::toggled, this, [this] { applyRenderOptions(); });
    connect(m_actionRenderLightweight, &QAction::toggled, this, [this] { applyRenderOptions(); });
}

void MainWindow::applyRenderOptions()
{
    if (!m_occView || !m_renderModeGroup || !m_renderModeGroup->checkedAction()) return;
    const auto mode = static_cast<RenderDisplayMode>(m_renderModeGroup->checkedAction()->data().toInt());
    const bool edges = m_actionRenderEdges->isChecked(), light = m_actionRenderLightweight->isChecked();
    QSettings settings;
    settings.setValue("view/renderMode", static_cast<int>(mode));
    settings.setValue("view/renderEdges", edges);
    settings.setValue("view/renderLightweight", light);
    m_occView->setRenderDisplayMode(mode);
    m_occView->setRenderOptions(edges, light);
    if (m_statusInfo)
        m_statusInfo->setText(tr("Rendu : %1%2%3")
                                  .arg(m_renderModeGroup->checkedAction()->text(), edges && !light ? tr(", arêtes") : QString(),
                                       light ? tr(", mode léger (sans texture ni arête)") : QString()));
}
