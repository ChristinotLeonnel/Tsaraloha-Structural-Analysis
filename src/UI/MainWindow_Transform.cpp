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

void MainWindow::onPointToPointMoveRequested(const gp_Pnt& base, const gp_Pnt& target, bool isCopy)
{
    if (!m_selectionManager || !m_model)
        return;

    m_model->pushUndoState(isCopy ? tr("Copie 3D").toStdString() : tr("Déplacement 3D").toStdString());

    double dx = target.X() - base.X();
    double dy = target.Y() - base.Y();
    double dz = target.Z() - base.Z();

    if (!isCopy)
    {
        const std::set<int> nodesToMove = selectionNodeClosure(*m_model, *m_selectionManager);

        if (!nodesToMove.empty())
        {
            auto cmd = std::make_unique<TSA::Commands::MoveElementsCommand>(*m_model, nodesToMove, dx, dy, dz);
            bool ok = m_commandManager ? m_commandManager->executeCommand(std::move(cmd)) : m_model->moveNodes(nodesToMove, dx, dy, dz);
            if (ok)
            {
                if (m_modelTree) m_modelTree->refreshAll();
                if (m_occView) m_occView->update();
                if (m_statusInfo)
                {
                    m_statusInfo->setText(tr("Déplacement 3D : %1 nœud(s) déplacé(s) de (%2, %3, %4) m")
                        .arg(nodesToMove.size())
                        .arg(dx, 0, 'f', 3)
                        .arg(dy, 0, 'f', 3)
                        .arg(dz, 0, 'f', 3));
                }
            }
        }
    }
    else
    {
        auto newIds = m_model->copyElements(
            m_selectionManager->selectedNodes(),
            m_selectionManager->selectedBeams(),
            m_selectionManager->selectedColumns(),
            m_selectionManager->selectedSlabs(),
            dx, dy, dz, 1,
            m_selectionManager->selectedCables(),
            m_selectionManager->selectedTrussMembers()
        );
        if (!newIds.empty())
        {
            if (m_modelTree) m_modelTree->refreshAll();
            if (m_occView) m_occView->update();
            if (m_statusInfo)
            {
                m_statusInfo->setText(tr("Copie 3D : %1 élément(s) créé(s) par translation")
                    .arg(newIds.size()));
            }
        }
    }
    updateUndoRedoActions();
}

void MainWindow::onPointToPointRotateRequested(const gp_Pnt& center, double angleRad, bool isCopy)
{
    if (!m_selectionManager || !m_model)
        return;

    m_model->pushUndoState(isCopy ? tr("Copie & Rotation 3D").toStdString() : tr("Rotation 3D").toStdString());

    gp_Dir axis(0.0, 0.0, 1.0);
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    double deg = angleRad * kRadToDeg;

    if (!isCopy)
    {
        const std::set<int> nodesToRotate = selectionNodeClosure(*m_model, *m_selectionManager);

        if (!nodesToRotate.empty())
        {
            auto cmd = std::make_unique<TSA::Commands::RotateElementsCommand>(*m_model, nodesToRotate, center.X(), center.Y(), center.Z(), deg, axis.X(), axis.Y(), axis.Z());
            bool ok = m_commandManager ? m_commandManager->executeCommand(std::move(cmd)) : m_model->rotateNodes(nodesToRotate, center, axis, angleRad);
            if (ok)
            {
                if (m_modelTree) m_modelTree->refreshAll();
                if (m_occView) m_occView->update();
                if (m_statusInfo)
                {
                    m_statusInfo->setText(tr("Rotation 3D : %1 nœud(s) tourné(s) de %2°")
                        .arg(nodesToRotate.size())
                        .arg(deg, 0, 'f', 1));
                }
            }
        }
    }
    else
    {
        const auto newIds = m_model->copyAndRotateElements(
            m_selectionManager->selectedNodes(), m_selectionManager->selectedBeams(),
            m_selectionManager->selectedColumns(), m_selectionManager->selectedSlabs(),
            center, axis, angleRad, 1,
            m_selectionManager->selectedCables(), m_selectionManager->selectedTrussMembers());
        if (m_statusInfo)
            m_statusInfo->setText(tr("Copie & rotation 3D : %1 élément(s) créé(s), %2°").arg(newIds.size()).arg(deg, 0, 'f', 1));
    }
    updateUndoRedoActions();
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
