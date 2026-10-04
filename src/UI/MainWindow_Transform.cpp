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

// =========================================================================
// Symétrie, division de barres, fusion de nœuds
// =========================================================================

void MainWindow::onActionMirror()
{
    if (!m_model || !m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Symétrie"),
            tr("Veuillez d'abord sélectionner les éléments à symétriser (nœuds, poutres, poteaux, dalles ou câbles)."));
        return;
    }

    const std::set<int> selNodes = m_selectionManager->selectedNodes();
    const std::set<int> selBeams = m_selectionManager->selectedBeams();
    const std::set<int> selColumns = m_selectionManager->selectedColumns();
    const std::set<int> selSlabs = m_selectionManager->selectedSlabs();
    const std::set<int> selCables = m_selectionManager->selectedCables();

    // Boîte englobante de la sélection : sert à proposer la position du plan.
    std::set<int> nodeIds = selNodes;
    for (int id : selBeams) if (const auto* b = m_model->getBeam(id)) { nodeIds.insert(b->startNodeId()); nodeIds.insert(b->endNodeId()); }
    for (int id : selColumns) if (const auto* c = m_model->getColumn(id)) { nodeIds.insert(c->startNodeId()); nodeIds.insert(c->endNodeId()); }
    for (int id : selSlabs) if (const auto* sl = m_model->getSlab(id)) nodeIds.insert(sl->nodeIds().begin(), sl->nodeIds().end());
    for (int id : selCables) if (const auto* c = m_model->getCable(id)) { nodeIds.insert(c->startNodeId()); nodeIds.insert(c->endNodeId()); }
    if (nodeIds.empty())
    {
        QMessageBox::information(this, tr("Symétrie"), tr("La sélection ne contient aucun élément symétrisable."));
        return;
    }
    double hi[3] = { -1e300, -1e300, -1e300 };
    for (int id : nodeIds)
    {
        if (const auto* n = m_model->getNode(id))
        {
            hi[0] = std::max(hi[0], n->x());
            hi[1] = std::max(hi[1], n->y());
            hi[2] = std::max(hi[2], n->z());
        }
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Symétrie (Miroir)"));
    auto* form = new QFormLayout(&dlg);
    auto* planeCombo = new QComboBox(&dlg);
    planeCombo->addItem(tr("Plan YZ  (X = constante)"));
    planeCombo->addItem(tr("Plan XZ  (Y = constante)"));
    planeCombo->addItem(tr("Plan XY  (Z = constante)"));
    auto* coordSpin = new QDoubleSpinBox(&dlg);
    coordSpin->setRange(-1e6, 1e6);
    coordSpin->setDecimals(3);
    coordSpin->setSuffix(tr(" m"));
    auto* keepCheck = new QCheckBox(tr("Conserver l'original (copie miroir)"), &dlg);
    keepCheck->setChecked(true);
    // Par défaut : plan au bord maximal de la sélection (copie accolée, nœuds du bord partagés).
    auto updateCoord = [planeCombo, coordSpin, &hi]() { coordSpin->setValue(hi[planeCombo->currentIndex()]); };
    QObject::connect(planeCombo, &QComboBox::currentIndexChanged, &dlg, updateCoord);
    updateCoord();
    form->addRow(tr("Plan de symétrie :"), planeCombo);
    form->addRow(tr("Position du plan :"), coordSpin);
    form->addRow(keepCheck);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const int axis = planeCombo->currentIndex();
    const double c = coordSpin->value();
    const bool keep = keepCheck->isChecked();
    const gp_Pnt planePoint(axis == 0 ? c : 0.0, axis == 1 ? c : 0.0, axis == 2 ? c : 0.0);
    const gp_Dir normal(axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0, axis == 2 ? 1.0 : 0.0);
    const char axisName = "XYZ"[axis];

    TSA::UndoRedo::EditTransaction tx(*m_model, (keep ? tr("Copie miroir") : tr("Symétrie")).toStdString());
    const auto ids = m_model->mirrorElements(selNodes, selBeams, selColumns, selSlabs,
                                             planePoint, normal, keep, selCables);
    if (ids.empty())
    {
        tx.rollback();
        if (m_statusInfo) m_statusInfo->setText(tr("Symétrie : aucun élément modifié (sélection entièrement sur le plan)."));
        return;
    }
    tx.record({ keep ? "create" : "move", "Selection", -1, "mirror", "",
                std::string(1, axisName) + " = " + QString::number(c, 'f', 3).toStdString() + " m",
                { "geometry", "results_invalidated" } });
    tx.commit();

    if (m_modelTree) m_modelTree->refreshAll();
    if (m_occView) m_occView->update();
    if (m_statusInfo)
    {
        m_statusInfo->setText(keep
            ? tr("Copie miroir (%1 = %2 m) : %3 élément(s) créé(s)").arg(QChar(axisName)).arg(c, 0, 'f', 3).arg(ids.size())
            : tr("Symétrie (%1 = %2 m) : %3 nœud(s) déplacé(s)").arg(QChar(axisName)).arg(c, 0, 'f', 3).arg(ids.size()));
    }
    updateUndoRedoActions();
}

void MainWindow::onActionSplitBars()
{
    if (!m_model || !m_selectionManager)
        return;

    const std::set<int> beams = m_selectionManager->selectedBeams();
    const std::set<int> columns = m_selectionManager->selectedColumns();
    if (beams.empty() && columns.empty())
    {
        QMessageBox::information(this, tr("Diviser les barres"),
            tr("Veuillez d'abord sélectionner les poutres et/ou poteaux à diviser."));
        return;
    }

    bool ok = false;
    const int segments = QInputDialog::getInt(this, tr("Diviser les barres"),
        tr("Nombre de tronçons égaux pour %1 barre(s) :").arg(beams.size() + columns.size()),
        2, 2, 100, 1, &ok);
    if (!ok)
        return;

    TSA::UndoRedo::EditTransaction tx(*m_model, tr("Division de barres").toStdString());
    int splitCount = 0;
    QStringList refused;
    for (int id : beams)
    {
        if (!m_model->splitBeam(id, segments).empty())
        {
            ++splitCount;
            tx.record({ "split", "Beam", id, "segments", "1", std::to_string(segments), { "geometry", "results_invalidated" } });
        }
        else
        {
            refused << tr("Poutre %1").arg(id);
        }
    }
    for (int id : columns)
    {
        if (!m_model->splitColumn(id, segments).empty())
        {
            ++splitCount;
            tx.record({ "split", "Column", id, "segments", "1", std::to_string(segments), { "geometry", "results_invalidated" } });
        }
        else
        {
            refused << tr("Poteau %1").arg(id);
        }
    }

    if (splitCount == 0)
    {
        tx.rollback();
    }
    else
    {
        tx.commit();
        m_selectionManager->clearSelection();
        if (m_modelTree) m_modelTree->refreshAll();
        if (m_occView) m_occView->update();
        if (m_statusInfo)
            m_statusInfo->setText(tr("Division : %1 barre(s) divisée(s) en %2 tronçons").arg(splitCount).arg(segments));
    }
    if (!refused.isEmpty())
    {
        QMessageBox::warning(this, tr("Diviser les barres"),
            tr("Barres non divisées (longueur nulle, ou charge ponctuelle / partielle / trapézoïdale "
               "qui ne peut pas être répartie sans ambiguïté) :\n%1").arg(refused.join(", ")));
    }
    updateUndoRedoActions();
}

void MainWindow::onActionMergeNodes()
{
    if (!m_model)
        return;

    bool ok = false;
    const double tolMm = QInputDialog::getDouble(this, tr("Fusionner les nœuds confondus"),
        tr("Tolérance de fusion (mm) :"), 1.0, 0.001, 1000.0, 3, &ok);
    if (!ok)
        return;
    const double tol = tolMm / 1000.0;

    const auto duplicates = m_model->findCoincidentNodes(tol);
    if (duplicates.empty())
    {
        QMessageBox::information(this, tr("Fusionner les nœuds confondus"),
            tr("Aucun nœud confondu à %1 mm près.").arg(tolMm));
        return;
    }
    if (QMessageBox::question(this, tr("Fusionner les nœuds confondus"),
            tr("%1 nœud(s) confondu(s) seront fusionnés avec le nœud de plus petit numéro.\n"
               "Les éléments, appuis et charges sont reportés ; les éléments devenus de longueur "
               "nulle sont supprimés.\n\nContinuer ?").arg(duplicates.size())) != QMessageBox::Yes)
        return;

    if (m_selectionManager) m_selectionManager->clearSelection();

    TSA::UndoRedo::EditTransaction tx(*m_model, tr("Fusion de nœuds").toStdString());
    const int merged = m_model->mergeCoincidentNodes(tol);
    for (const auto& [dupId, keeperId] : duplicates)
        tx.record({ "merge", "Node", dupId, "node", std::to_string(dupId), std::to_string(keeperId), { "topology", "results_invalidated" } });
    tx.commit();

    if (m_modelTree) m_modelTree->refreshAll();
    if (m_occView) m_occView->update();
    if (m_statusInfo)
        m_statusInfo->setText(tr("Fusion : %1 nœud(s) supprimé(s) (tolérance %2 mm)").arg(merged).arg(tolMm));
    updateUndoRedoActions();
}
