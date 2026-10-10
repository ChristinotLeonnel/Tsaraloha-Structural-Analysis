// Paramètres du projet → Topologie et numérotation (fenêtre TopologyDialog, service src/Topology).

#include "MainWindow.h"

#include "Dialogs/TopologyDialog.h"
#include "../Coordinate/LevelManager.h"
#include "../Grid/GridDefinition.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridSystem.h"
#include "../Model/Model.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"

#include <QAction>

using TSA::Topology::EntityFamily;

TSA::Topology::NumberingInput MainWindow::topologyNumberingInput() const
{
    TSA::Topology::NumberingInput input;
    if (m_gridManager)
    {
        if (const auto* grid = m_gridManager->activeGrid(); grid && grid->type() == TSA::Grid::GridType::Cartesian)
        {
            const auto& d = grid->definition();
            TSA::Topology::GridAxes g;
            g.originX = d.origin().X();
            g.originY = d.origin().Y();
            g.rotationDeg = d.rotationDeg();
            g.xPositions = d.xPositions();
            g.xLabels = d.xLabels();
            g.yPositions = d.yPositions();
            g.yLabels = d.yLabels();
            input.grid = g;
        }
    }
    if (m_model && m_model->levelManager()) input.levelElevations = m_model->levelManager()->elevationList();
    if (m_selectionManager)
    {
        input.selectedNodes = m_selectionManager->selectedNodes();
        input.selectedElements[EntityFamily::Beam] = m_selectionManager->selectedBeams();
        input.selectedElements[EntityFamily::Column] = m_selectionManager->selectedColumns();
        input.selectedElements[EntityFamily::Truss] = m_selectionManager->selectedTrussMembers();
        input.selectedElements[EntityFamily::Cable] = m_selectionManager->selectedCables();
        input.selectedElements[EntityFamily::Slab] = m_selectionManager->selectedSlabs();
        input.selectedElements[EntityFamily::Wall] = m_selectionManager->selectedWalls();
        input.selectedElements[EntityFamily::Foundation] = m_selectionManager->selectedFoundations();
    }
    return input;
}

void MainWindow::onActionTopologySettings()
{
    if (!m_model) return;
    // Non modale : la sélection dans la vue reste possible (portée « Entités sélectionnées »).
    auto* dlg = new TSA::UI::TopologyDialog(m_model, [this] { return topologyNumberingInput(); }, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &TSA::UI::TopologyDialog::settingsApplied, this, [this](const TSA::Topology::TopologySettings& s) {
        applyTopologyDisplay(s);
        updateUndoRedoActions();
        updateWindowTitle();
    });
    dlg->show();
}

void MainWindow::applyTopologyDisplay(const TSA::Topology::TopologySettings& settings)
{
    if (m_occView) m_occView->setNodeLabelsVisible(settings.showNodeLabels);
    if (m_actionNodeLabelsVisible)
    {
        const QSignalBlocker block(m_actionNodeLabelsVisible);
        m_actionNodeLabelsVisible->setChecked(settings.showNodeLabels);
    }
}

void MainWindow::applyTopologyDisplayFromModel()
{
    // Anciens projets (sans chunk TOPO) : l'affichage courant est conservé.
    if (!m_model || m_model->topologySettingsJson().empty()) return;
    applyTopologyDisplay(TSA::Topology::TopologySettings::fromJson(m_model->topologySettingsJson()));
}
