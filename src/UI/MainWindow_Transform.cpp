#include "MainWindow.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "../Model/Model.h"
#include "../Grid/GridManager.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Ruler/ViewportContainer.h"
#include "../UndoRedo/CommandManager.h"
#include "../Commands/ModifyCommands.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <set>

// =========================================================================
// Transformations 3D Directes & Presse-papier
// =========================================================================

void MainWindow::onActionMove3D()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Déplacement 3D"),
            tr("Veuillez d'abord sélectionner les éléments à déplacer (nœuds, barres, poteaux ou dalles)."));
        if (m_actionSelectMode) m_actionSelectMode->setChecked(true);
        return;
    }
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Move3D);
    }
}

void MainWindow::onActionCopy3D()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Copie 3D"),
            tr("Veuillez d'abord sélectionner les éléments à copier (nœuds, barres, poteaux ou dalles)."));
        if (m_actionSelectMode) m_actionSelectMode->setChecked(true);
        return;
    }
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Copy3D);
    }
}

void MainWindow::onActionRotate3D()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Rotation 3D"),
            tr("Veuillez d'abord sélectionner les éléments à faire tourner."));
        if (m_actionSelectMode) m_actionSelectMode->setChecked(true);
        return;
    }
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Rotate3D);
    }
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
        std::set<int> nodesToMove(
            m_selectionManager->selectedNodes().begin(),
            m_selectionManager->selectedNodes().end()
        );
        for (int bId : m_selectionManager->selectedBeams())
        {
            const auto* b = m_model->getBeam(bId);
            if (b) { nodesToMove.insert(b->startNodeId()); nodesToMove.insert(b->endNodeId()); }
        }
        for (int cId : m_selectionManager->selectedColumns())
        {
            const auto* c = m_model->getColumn(cId);
            if (c) { nodesToMove.insert(c->startNodeId()); nodesToMove.insert(c->endNodeId()); }
        }
        for (int sId : m_selectionManager->selectedSlabs())
        {
            const auto* s = m_model->getSlab(sId);
            if (s)
            {
                for (int nid : s->nodeIds()) nodesToMove.insert(nid);
            }
        }

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
            dx, dy, dz, 1
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
        std::set<int> nodesToRotate(
            m_selectionManager->selectedNodes().begin(),
            m_selectionManager->selectedNodes().end()
        );
        for (int bId : m_selectionManager->selectedBeams())
        {
            const auto* b = m_model->getBeam(bId);
            if (b) { nodesToRotate.insert(b->startNodeId()); nodesToRotate.insert(b->endNodeId()); }
        }
        for (int cId : m_selectionManager->selectedColumns())
        {
            const auto* c = m_model->getColumn(cId);
            if (c) { nodesToRotate.insert(c->startNodeId()); nodesToRotate.insert(c->endNodeId()); }
        }
        for (int sId : m_selectionManager->selectedSlabs())
        {
            const auto* s = m_model->getSlab(sId);
            if (s)
            {
                for (int nid : s->nodeIds()) nodesToRotate.insert(nid);
            }
        }

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
