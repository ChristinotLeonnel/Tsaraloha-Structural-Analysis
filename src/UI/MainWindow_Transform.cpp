#include "MainWindow.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "../Model/Model.h"
#include "../Grid/GridManager.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Ruler/ViewportContainer.h"
#include "../UndoRedo/CommandManager.h"
#include "../Commands/ModifyCommands.h"
#include "../UndoRedo/EditTransaction.h"
#include "Tools/ModelCleanupDialog.h"
#include "Dock/LogConsoleDock.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QInputDialog>
#include <algorithm>
#include <set>

namespace
{
/// Nœuds de la sélection : nœuds sélectionnés + extrémités / sommets de tous les éléments
/// sélectionnés (barres, poteaux, treillis, câbles, dalles).
std::set<int> selectionNodeClosure(const TSA::Model::Model& model, const TSA::Viewer::SelectionManager& sel)
{
    std::set<int> nodes(sel.selectedNodes().begin(), sel.selectedNodes().end());
    auto ends = [&](int a, int b) { nodes.insert(a); nodes.insert(b); };
    for (int id : sel.selectedBeams()) if (const auto* e = model.getBeam(id)) ends(e->startNodeId(), e->endNodeId());
    for (int id : sel.selectedColumns()) if (const auto* e = model.getColumn(id)) ends(e->startNodeId(), e->endNodeId());
    for (int id : sel.selectedTrussMembers()) if (const auto* e = model.getTrussMember(id)) ends(e->startNodeId(), e->endNodeId());
    for (int id : sel.selectedCables()) if (const auto* e = model.getCable(id)) ends(e->startNodeId(), e->endNodeId());
    for (int id : sel.selectedSlabs()) if (const auto* e = model.getSlab(id)) nodes.insert(e->nodeIds().begin(), e->nodeIds().end());
    return nodes;
}
} // namespace

// =========================================================================
// Transformations 3D Directes & Presse-papier
// =========================================================================

void MainWindow::onActionMove3D()
{
    startModelingTool("move");
}

void MainWindow::onActionCopy3D()
{
    startModelingTool("copy");
}

void MainWindow::onActionRotate3D()
{
    startModelingTool("rotate");
}

void MainWindow::onActionMoveOrigin()
{
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::MoveOrigin3D);
    }
}

void MainWindow::onActionCopyClipboard()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection() || !m_model)
    {
        if (statusBar()) statusBar()->showMessage(tr("Presse-papier : Aucun élément sélectionné."), 3000);
        return;
    }

    m_clipboard.copyFrom(*m_model,
                         m_selectionManager->selectedNodes(),
                         m_selectionManager->selectedBeams(),
                         m_selectionManager->selectedColumns(),
                         m_selectionManager->selectedSlabs());

    if (statusBar())
    {
        statusBar()->showMessage(tr("Presse-papier : %1 nœud(s), %2 barre(s) copiés (Ctrl+V pour coller)")
            .arg(m_clipboard.nodeCount())
            .arg(m_clipboard.totalElementCount()), 4000);
    }
}

void MainWindow::onActionPasteClipboard()
{
    if (!m_clipboard.hasData())
    {
        if (statusBar()) statusBar()->showMessage(tr("Presse-papier vide. Sélectionnez des éléments et faites Ctrl+C."), 3000);
        return;
    }
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Paste3D);
    }
}

void MainWindow::onOriginMoveRequested(const gp_Pnt& newOrigin)
{
    if (m_gridManager)
    {
        if (auto* grid = m_gridManager->activeGrid())
        {
            auto gdef = grid->definition();
            gdef.setOrigin(newOrigin.X(), newOrigin.Y(), newOrigin.Z());
            grid->updateDefinition(gdef);
        }
    }
    if (m_occView)
    {
        m_occView->rebuildGrid();
        m_occView->update();
        m_occView->setInteractionMode(OccView::InteractionMode::Select);
    }
    if (m_viewportContainer)
    {
        m_viewportContainer->updateRulers();
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Repère Global & Grille déplacés en (%1, %2, %3) m")
            .arg(newOrigin.X(), 0, 'f', 3)
            .arg(newOrigin.Y(), 0, 'f', 3)
            .arg(newOrigin.Z(), 0, 'f', 3));
    }
}

void MainWindow::onPasteAtPointRequested(const gp_Pnt& target)
{
    if (!m_clipboard.hasData() || !m_model)
        return;

    m_model->pushUndoState(tr("Coller Presse-papier").toStdString());

    auto res = m_clipboard.pasteTo(*m_model, target.X(), target.Y(), target.Z());
    if (res.empty())
        return;

    if (m_selectionManager)
    {
        m_selectionManager->clearSelection();
        for (int nid : res.nodeIds) m_selectionManager->selectNode(nid, true);
        for (int bid : res.beamIds) m_selectionManager->selectBeam(bid, true);
        for (int cid : res.columnIds) m_selectionManager->selectColumn(cid, true);
        for (int sid : res.slabIds) m_selectionManager->selectSlab(sid, true);
    }

    if (m_modelTree) m_modelTree->refreshAll();
    if (m_occView)
    {
        m_occView->update();
        m_occView->setInteractionMode(OccView::InteractionMode::Select);
    }

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Collé en (%1, %2, %3) m : %4 élément(s)")
            .arg(target.X(), 0, 'f', 2)
            .arg(target.Y(), 0, 'f', 2)
            .arg(target.Z(), 0, 'f', 2)
            .arg(res.nodeIds.size() + res.beamIds.size() + res.columnIds.size() + res.slabIds.size()));
    }
    updateUndoRedoActions();
}

// =========================================================================
// Symétrie, division de barres, fusion de nœuds
// =========================================================================

void MainWindow::onActionMirror()
{
    startModelingTool("mirror");
}

void MainWindow::onActionSplitBars()
{
    startModelingTool("split");
}

void MainWindow::onActionMergeNodes()
{
    startModelingTool("merge_nodes");
}

// =========================================================================
// Nettoyage topologique du modèle
// =========================================================================

std::string MainWindow::applyModelCleanup(const TSA::Model::CleanupOptions& options)
{
    if (!m_model) return {};
    TSA::UndoRedo::EditTransaction tx(*m_model, tr("Nettoyage du modèle").toStdString());
    const TSA::Model::CleanupReport report = TSA::Model::ModelCleanup::clean(*m_model, options);
    if (!report.changed())
    {
        tx.rollback();
        return {};
    }
    tx.record({ "modify", "Model", -1, "cleanup", "", report.summary(), { "topology", "results_invalidated" } });
    tx.commit();

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("--- NETTOYAGE DU MODÈLE ---"), "SYS");
        for (const auto& d : report.details) m_consoleDock->appendLog(QString::fromStdString(d), "INFO");
        for (const auto& d : report.refused) m_consoleDock->appendLog(QString::fromStdString(d), "WARN");
        for (const auto& d : report.warnings) m_consoleDock->appendLog(QString::fromStdString(d), "WARN");
        m_consoleDock->appendLog(QString::fromStdString(report.summary()), "SUCCESS");
    }
    if (m_selectionManager) m_selectionManager->clearSelection();
    if (m_modelTree) m_modelTree->refreshAll();
    if (m_occView) m_occView->update();
    if (m_statusInfo) m_statusInfo->setText(tr("Nettoyage : %1").arg(QString::fromStdString(report.summary())));
    updateUndoRedoActions();
    return report.summary();
}

void MainWindow::onActionCleanModel()
{
    if (!m_model) return;
    TSA::UI::ModelCleanupDialog dlg(*m_model, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const std::string summary = applyModelCleanup(dlg.options());
    if (!summary.empty())
        QMessageBox::information(this, tr("Nettoyage du modèle"),
                                 tr("%1\n\nAnnulable par Ctrl+Z ; le détail est dans la console.").arg(QString::fromStdString(summary)));
}
