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
    startModelingTool("move", false, m_actionMove3D);
}

void MainWindow::onActionCopy3D()
{
    startModelingTool("copy", false, m_actionCopy3D);
}

void MainWindow::onActionRotate3D()
{
    startModelingTool("rotate", false, m_actionRotate3D);
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

    TSA::Model::ClipboardSelection sel;
    sel.nodes = m_selectionManager->selectedNodes();
    sel.beams = m_selectionManager->selectedBeams();
    sel.columns = m_selectionManager->selectedColumns();
    sel.slabs = m_selectionManager->selectedSlabs();
    sel.walls = m_selectionManager->selectedWalls();
    sel.trusses = m_selectionManager->selectedTrussMembers();
    sel.cables = m_selectionManager->selectedCables();
    TSA::Model::ClipboardOptions options;
    options.supports = !m_actionClipCopySupports || m_actionClipCopySupports->isChecked();
    options.loads = m_actionClipCopyLoads && m_actionClipCopyLoads->isChecked();
    // Presse-papier commun à tous les onglets : le collage peut se faire dans un autre projet ouvert
    m_clipboard.copyFrom(*m_model, sel, options);

    if (statusBar())
    {
        statusBar()->showMessage(tr("Presse-papier : %1 nœud(s), %2 élément(s), %3 charge(s) copiés — Ctrl+V au point désigné, Ctrl+Maj+V à la position d'origine, dans ce projet ou un autre onglet")
            .arg(m_clipboard.nodeCount())
            .arg(m_clipboard.totalElementCount())
            .arg(m_clipboard.loadCount()), 6000);
    }
}

void MainWindow::onActionCutClipboard()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection() || !m_model) return;
    onActionCopyClipboard();
    if (!m_clipboard.hasData()) return;
    onActionDeleteSelected();   // suppression annulable (Ctrl+Z restaure les éléments coupés)
}

void MainWindow::onActionPasteOriginal()
{
    if (!m_clipboard.hasData())
    {
        if (statusBar()) statusBar()->showMessage(tr("Presse-papier vide. Sélectionnez des éléments et faites Ctrl+C."), 3000);
        return;
    }
    pasteClipboardAt(m_clipboard.originX(), m_clipboard.originY(), m_clipboard.originZ(), tr("à la position d'origine"));
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
    pasteClipboardAt(target.X(), target.Y(), target.Z(),
                     tr("en (%1, %2, %3) m").arg(target.X(), 0, 'f', 2).arg(target.Y(), 0, 'f', 2).arg(target.Z(), 0, 'f', 2));
    if (m_occView) m_occView->setInteractionMode(OccView::InteractionMode::Select);
}

void MainWindow::pasteClipboardAt(double x, double y, double z, const QString& where)
{
    if (!m_clipboard.hasData() || !m_model)
        return;
    if (!confirmNoRunningAnalysis(tr("coller"))) return;

    // Une seule étape d'annulation ; tout ou rien (le modèle est rétabli si un élément ne peut pas être recréé)
    auto res = m_clipboard.pasteUndoable(*m_model, x, y, z, tr("Coller Presse-papier").toStdString());
    if (!res.complete)
    {
        QMessageBox::warning(this, tr("Coller"), tr("Le collage a été annulé : un élément n'a pas pu être recréé (%1). Le modèle est inchangé.")
                                                     .arg(QString::fromStdString(res.error)));
        updateUndoRedoActions();
        return;
    }
    if (res.empty())
        return;

    if (m_selectionManager)
    {
        m_selectionManager->clearSelection();
        for (int nid : res.nodeIds) m_selectionManager->selectNode(nid, true);
        for (int bid : res.beamIds) m_selectionManager->selectBeam(bid, true);
        for (int cid : res.columnIds) m_selectionManager->selectColumn(cid, true);
        for (int sid : res.slabIds) m_selectionManager->selectSlab(sid, true);
        for (int wid : res.wallIds) m_selectionManager->selectWall(wid, true);
        for (int tid : res.trussIds) m_selectionManager->selectTrussMember(tid, true);
        for (int cid : res.cableIds) m_selectionManager->selectCable(cid, true);
    }

    if (m_modelTree) m_modelTree->refreshAll();
    if (m_occView)
    {
        if (res.loadCount() > 0) m_occView->updateAllLoadShapes();
        m_occView->update();
    }

    if (m_statusInfo)
    {
        QString text = tr("Collé %1 : %2 nœud(s), %3 élément(s)").arg(where).arg(res.nodeIds.size()).arg(res.elementCount());
        if (res.loadCount() > 0) text += tr(", %1 charge(s)").arg(res.loadCount());
        if (!res.createdLoadCaseIds.empty()) text += tr(" — %1 cas de charge créé(s)").arg(res.createdLoadCaseIds.size());
        m_statusInfo->setText(text);
    }
    updateUndoRedoActions();
}

// =========================================================================
// Symétrie, division de barres, fusion de nœuds
// =========================================================================

void MainWindow::onActionMirror()
{
    startModelingTool("mirror", false, m_actionMirror);
}

void MainWindow::onActionSplitBars()
{
    startModelingTool("split", false, m_actionSplitBars);
}

void MainWindow::onActionMergeNodes()
{
    startModelingTool("merge_nodes", false, m_actionMergeNodes);
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
