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

#include <QSettings>
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
    sel.foundations = m_selectionManager->selectedFoundations();
    sel.dimensions = m_selectionManager->selectedDimensions();
    TSA::Model::ClipboardOptions options;
    options.supports = !m_actionClipCopySupports || m_actionClipCopySupports->isChecked();
    options.loads = m_actionClipCopyLoads && m_actionClipCopyLoads->isChecked();
    // Presse-papier commun à tous les onglets : le collage peut se faire dans un autre projet ouvert
    m_clipboard.copyFrom(*m_model, sel, options);
    if (m_consoleDock)
        for (const auto& w : m_clipboard.copyWarnings()) m_consoleDock->appendLog(tr("Copie : %1").arg(QString::fromStdString(w)), "WARN");

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

    TSA::Model::PasteOptions options;
    {
        QSettings settings;
        options.merge = m_actionClipMergeNodes && m_actionClipMergeNodes->isChecked() ? TSA::Model::NodeMergeMode::MergeCoincident
                                                                                     : TSA::Model::NodeMergeMode::KeepSeparate;
        options.tolerance = settings.value("Clipboard/mergeToleranceM", 1.0e-3).toDouble();
    }
    auto listOf = [](const std::vector<TSA::Model::PasteConflict>& conflicts) {
        QStringList lines;
        for (std::size_t i = 0; i < conflicts.size() && i < 12; ++i) lines << QStringLiteral("• ") + QString::fromStdString(conflicts[i].message);
        if (conflicts.size() > 12) lines << QObject::tr("… et %1 autre(s)").arg(conflicts.size() - 12);
        return lines.join('\n');
    };
    // Éléments identiques à des éléments existants (fusion) : décision avant le collage
    if (options.merge == TSA::Model::NodeMergeMode::MergeCoincident)
    {
        auto probe = options;
        probe.supportConflicts = TSA::Model::SupportConflictPolicy::KeepTarget;   // analyse des doublons seulement
        const auto plan = m_clipboard.analyzePaste(*m_model, x, y, z, probe);
        if (!plan.duplicates.empty() && !plan.blocked())
        {
            QMessageBox box(QMessageBox::Question, tr("Coller — éléments en double"),
                            tr("%1 élément(s) collé(s) relieraient des nœuds existants déjà reliés par un élément de la même famille :\n\n%2\n\n"
                               "Les coller doublerait leur rigidité.")
                                .arg(plan.duplicates.size()).arg(listOf(plan.duplicates)),
                            QMessageBox::NoButton, this);
            auto* skip = box.addButton(tr("Ne pas coller les doublons"), QMessageBox::AcceptRole);
            auto* keep = box.addButton(tr("Les coller quand même"), QMessageBox::DestructiveRole);
            box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(skip);
            box.exec();
            if (box.clickedButton() == keep) options.duplicates = TSA::Model::DuplicateElementPolicy::Keep;
            else if (box.clickedButton() != skip) return;
        }
    }

    // Une seule étape d'annulation ; tout ou rien (le modèle est rétabli si un élément ne peut pas être recréé)
    auto res = m_clipboard.pasteUndoable(*m_model, x, y, z, tr("Coller Presse-papier").toStdString(), options);
    if (!res.complete && res.needsDecision)
    {
        // Appuis incompatibles sur des nœuds coïncidents : aucune propriété n'est supprimée sans décision
        QMessageBox box(QMessageBox::Warning, tr("Coller — appuis incompatibles"),
                        tr("Des nœuds collés tombent sur des nœuds existants qui portent un appui différent :\n\n%1\n\nQue faire ?")
                            .arg(listOf(res.conflicts)),
                        QMessageBox::NoButton, this);
        auto* separate = box.addButton(tr("Ne pas fusionner ces nœuds"), QMessageBox::AcceptRole);
        auto* keepTarget = box.addButton(tr("Conserver les appuis du projet"), QMessageBox::ActionRole);
        auto* useCopied = box.addButton(tr("Utiliser les appuis copiés"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(separate);
        box.exec();
        if (box.clickedButton() == separate) options.supportConflicts = TSA::Model::SupportConflictPolicy::DontMerge;
        else if (box.clickedButton() == keepTarget) options.supportConflicts = TSA::Model::SupportConflictPolicy::KeepTarget;
        else if (box.clickedButton() == useCopied) options.supportConflicts = TSA::Model::SupportConflictPolicy::UseCopied;
        else return;   // annulé : modèle inchangé
        res = m_clipboard.pasteUndoable(*m_model, x, y, z, tr("Coller Presse-papier").toStdString(), options);
    }
    if (!res.complete)
    {
        QMessageBox::warning(this, tr("Coller"), tr("Le collage a été annulé, le modèle est inchangé :\n\n%1")
                                                     .arg(res.conflicts.empty() ? QString::fromStdString(res.error) : listOf(res.conflicts)));
        updateUndoRedoActions();
        return;
    }
    if (m_consoleDock)
        for (const auto& w : res.warnings) m_consoleDock->appendLog(tr("Collage : %1").arg(QString::fromStdString(w)), "WARN");
    if (res.empty() && res.loadCount() == 0)
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
        for (int fid : res.foundationIds) m_selectionManager->selectFoundation(fid, true);
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
        if (!res.mergedNodeIds.empty()) text += tr(" — %1 nœud(s) fusionné(s)").arg(res.mergedNodeIds.size());
        if (res.skippedDuplicates > 0) text += tr(" — %1 doublon(s) non collé(s)").arg(res.skippedDuplicates);
        if (!res.dimensionIds.empty()) text += tr(" — %1 cotation(s)").arg(res.dimensionIds.size());
        if (!res.combinationIds.empty()) text += tr(" — %1 combinaison(s)").arg(res.combinationIds.size());
        if (!res.warnings.empty()) text += tr(" (%1 avertissement(s) dans la console)").arg(res.warnings.size());
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
