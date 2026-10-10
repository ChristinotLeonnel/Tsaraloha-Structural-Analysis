// Représentation du modèle dans la vue 3D (Affichage > Représentation) : physique, filaire analytique,
// éléments finis, superposition. Visuel seulement : rien n'est modifié dans le modèle ni dans les
// résultats. Logique de rendu : src/Viewer/OccView_DisplayMode.cpp ; politique : ModelDisplayMode.h.

#include "MainWindow.h"

#include "Dock/LogConsoleDock.h"
#include "../Viewer/OccView.h"

#include <QAction>
#include <QActionGroup>
#include <QLabel>

using TSA::Viewer::ModelDisplayMode;

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
        { ModelDisplayMode::Physical, ":/icons/view/view_shaded.svg", tr("Modèle\nphysique"),
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
        auto* a = new QAction(QIcon(d.icon), d.text, this);
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
    QString text = tr("Représentation : %1").arg(QString::fromUtf8(TSA::Viewer::modelDisplayModeName(mode)));
    if (mode == ModelDisplayMode::FiniteElement)
    {
        const auto status = m_occView->solverMeshStatus();
        text += QStringLiteral(" — ") + QString::fromStdString(status.message);
        if (m_consoleDock) m_consoleDock->appendLog(QString::fromStdString(status.message), status.available ? "INFO" : "WARN");
    }
    if (m_statusInfo) m_statusInfo->setText(text);
}
