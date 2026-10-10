// Outils de modification et de dessin (TSA::Interaction::ModelingTool) :
// saisie dans la vue 3D (par défaut) ou par fenêtre, exécution en une transaction.

#include "MainWindow.h"

#include "Dock/LogConsoleDock.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Tools/ModelingToolDialog.h"
#include "../Grid/GridManager.h"
#include "../Interaction/Tools/ModelingTool.h"
#include "../Model/Model.h"
#include "../UndoRedo/EditTransaction.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"

#include <QSignalBlocker>
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>

namespace
{
const char* kInputModeKey = "modeling/inputInViewport";

QString iconFor(const std::string& id)
{
    static const std::map<std::string, QString> icons {
        { "move", ":/icons/structure/struct_move.svg" },
        { "copy", ":/icons/structure/struct_copy.svg" },
        { "rotate", ":/icons/edit/rotate.svg" },
        { "mirror", ":/icons/edit/mirror.svg" },
        { "split", ":/icons/structure/struct_split.svg" },
        { "merge_nodes", ":/icons/structure/struct_merge.svg" },
        { "scale", ":/icons/tools/scale.svg" },
        { "array_linear", ":/icons/tools/array_linear.svg" },
        { "array_polar", ":/icons/tools/array_polar.svg" },
        { "offset", ":/icons/tools/offset.svg" },
        { "split_at", ":/icons/tools/split_at.svg" },
        { "intersect", ":/icons/tools/intersect.svg" },
        { "extend", ":/icons/tools/extend.svg" },
        { "trim", ":/icons/tools/trim.svg" },
        { "draw_beam_chain", ":/icons/tools/beam_chain.svg" },
        { "draw_beam_rectangle", ":/icons/tools/beam_rectangle.svg" },
        { "draw_portal", ":/icons/tools/portal.svg" },
        { "draw_x_bracing", ":/icons/tools/x_bracing.svg" },
        { "draw_beam_arc", ":/icons/tools/beam_arc.svg" },
        { "draw_grid_columns", ":/icons/tools/grid_columns.svg" },
        { "dim_aligned", ":/icons/dimensions/dim_aligned.svg" },
        { "dim_linear", ":/icons/dimensions/dim_linear.svg" },
        { "dim_horizontal", ":/icons/dimensions/dim_horizontal.svg" },
        { "dim_x", ":/icons/dimensions/dim_x.svg" },
        { "dim_y", ":/icons/dimensions/dim_y.svg" },
        { "dim_z", ":/icons/dimensions/dim_z.svg" },
        { "dim_angular", ":/icons/dimensions/dim_angular.svg" },
        { "dim_level", ":/icons/dimensions/dim_level.svg" },
        { "dim_chain", ":/icons/dimensions/dim_chain.svg" },
        { "dim_cumulative", ":/icons/dimensions/dim_cumulative.svg" },
    };
    auto it = icons.find(id);
    return it != icons.end() ? it->second : QString();
}
} // namespace

void MainWindow::createModelingToolActions()
{
    m_toolRegistry = std::make_unique<TSA::Interaction::ModelingToolRegistry>();
    TSA::Interaction::registerBuiltInModelingTools(*m_toolRegistry);

    m_toolInputInViewport = QSettings().value(kInputModeKey, true).toBool();
    m_actionToolInputViewport = new QAction(QIcon(":/icons/tools/input_viewport.svg"), tr("Saisie dans la vue 3D"), this);
    m_actionToolInputViewport->setCheckable(true);
    m_actionToolInputViewport->setChecked(m_toolInputInViewport);
    m_actionToolInputViewport->setToolTip(tr("Coché : les outils de modification et de dessin se pilotent directement dans la vue 3D "
                                             "(clics, valeur tapée au clavier + Entrée).\nDécoché : ils ouvrent une fenêtre de paramètres.\n"
                                             "Maj + clic sur un outil : utiliser l'autre mode une fois."));
    connect(m_actionToolInputViewport, &QAction::toggled, this, [this](bool on) {
        m_toolInputInViewport = on;
        QSettings().setValue(kInputModeKey, on);
        if (m_statusInfo)
            m_statusInfo->setText(on ? tr("Outils : saisie directe dans la vue 3D") : tr("Outils : saisie par fenêtre de paramètres"));
    });

    for (const auto& tool : m_toolRegistry->instances())
    {
        const std::string id = tool->id();
        auto* action = new QAction(QIcon(iconFor(id)), QString::fromStdString(tool->name()), this);
        QString tip = QString::fromStdString(tool->description());
        if (!tool->supportsDialog()) tip += tr("\n(Saisie dans la vue 3D uniquement.)");
        action->setToolTip(tip);
        action->setStatusTip(tip);
        action->setCheckable(true);   // coloré pendant la commande (syncToolActionStates)
        connect(action, &QAction::triggered, this, [this, id, action]() {
            const bool shift = QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
            startModelingTool(id, shift, action);
        });
        m_toolActions[id] = action;
    }
}

TSA::Interaction::ToolContext MainWindow::modelingToolContext() const
{
    TSA::Interaction::ToolContext ctx;
    if (m_selectionManager) ctx.selection = m_selectionManager->selectedElements();
    if (m_occView)
    {
        const auto& wp = m_occView->activeWorkPlane();
        ctx.planeOrigin = wp.origin();
        ctx.planeNormal = wp.normal();
        ctx.planeX = wp.axisX();
    }
    ctx.presets = m_presets;
    ctx.grids = m_gridManager;
    return ctx;
}

void MainWindow::startModelingTool(const std::string& id, bool swapInputMode, QAction* launcher)
{
    startModelingToolImpl(id, swapInputMode, launcher);
    // Toutes les issues (refus faute de sélection, fenêtre fermée, application immédiate) : un clic sur
    // un bouton cochable l'a basculé, son état est ramené à celui de la commande.
    syncToolActionStates();
}

void MainWindow::syncToolActionStates()
{
    const bool viewportTool = m_occView && m_activeTool
                              && m_occView->interactionMode() == OccView::InteractionMode::ModelingTool
                              && m_occView->activeModelingTool() == m_activeTool.get();
    const bool running = viewportTool || (m_toolDialogRunning && m_activeTool);
    if (!running) m_toolLauncher = nullptr;

    auto show = [](QAction* a, bool on) {
        if (!a || !a->isCheckable() || a->isChecked() == on) return;
        const QSignalBlocker block(a);
        a->setChecked(on);
    };
    std::vector<QAction*> launchers = { m_actionMove3D, m_actionCopy3D, m_actionRotate3D, m_actionMove, m_actionCopy,
                                        m_actionMirror, m_actionSplitBars, m_actionMergeNodes };
    for (const auto& [toolId, action] : m_toolActions) launchers.push_back(action);
    for (QAction* a : launchers) show(a, running && a == m_toolLauncher);

    show(m_actionMoveOrigin, m_occView && m_occView->interactionMode() == OccView::InteractionMode::MoveOrigin3D);

    // Boutons de mode (groupe exclusif facultatif) : signaux non bloqués, le groupe tient son état à jour.
    QAction* modeAction = nullptr;
    if (m_occView)
    {
        switch (m_occView->interactionMode())
        {
        case OccView::InteractionMode::Select: modeAction = m_actionSelectMode; break;
        case OccView::InteractionMode::DrawNode: modeAction = m_actionDrawNode; break;
        case OccView::InteractionMode::DrawBar: modeAction = m_actionDrawBar; break;
        case OccView::InteractionMode::DrawBeam: modeAction = m_actionDrawBeam; break;
        case OccView::InteractionMode::DrawColumn: modeAction = m_actionDrawColumn; break;
        case OccView::InteractionMode::DrawSlab: modeAction = m_actionDrawSlab; break;
        case OccView::InteractionMode::DrawWall: modeAction = m_actionDrawWall; break;
        case OccView::InteractionMode::DrawCable:
        case OccView::InteractionMode::DrawStayCable:
        case OccView::InteractionMode::DrawSuspensionCable:
        case OccView::InteractionMode::DrawHanger: modeAction = m_actionDrawCable; break;
        default: break;   // outils, origine, fondation, treillis, collage : aucun bouton de mode
        }
    }
    for (QAction* a : { m_actionSelectMode, m_actionDrawNode, m_actionDrawBar, m_actionDrawBeam, m_actionDrawColumn,
                        m_actionDrawCable, m_actionDrawSlab, m_actionDrawWall })
        if (a && a != modeAction && a->isChecked()) a->setChecked(false);
    if (modeAction && !modeAction->isChecked()) modeAction->setChecked(true);
}

void MainWindow::startModelingToolImpl(const std::string& id, bool swapInputMode, QAction* launcher)
{
    if (!m_model || !m_toolRegistry) return;
    if (m_occView && m_occView->activeModelingTool())
        m_occView->setInteractionMode(OccView::InteractionMode::Select);   // la vue lâche l'outil courant
    m_activeTool = m_toolRegistry->create(id);
    if (!m_activeTool) return;
    // Après le relâchement de l'outil précédent (qui remet le bouton précédent à zéro).
    if (!launcher)
        if (const auto it = m_toolActions.find(id); it != m_toolActions.end()) launcher = it->second;
    m_toolLauncher = launcher;

    const TSA::Interaction::ToolContext ctx = modelingToolContext();
    const QString name = QString::fromStdString(m_activeTool->name());
    if (m_activeTool->needsSelection() && ctx.selection.empty())
    {
        m_activeTool.reset();
        syncToolActionStates();   // bouton décoloré avant le message : la commande n'a pas démarré
        QMessageBox::information(this, name, tr("Sélectionnez d'abord les éléments à traiter, puis relancez « %1 ».").arg(name));
        return;
    }
    m_activeTool->prepare(ctx);

    const bool viewport = (m_toolInputInViewport != swapInputMode) || !m_activeTool->supportsDialog();
    if (viewport && m_occView)
    {
        m_occView->startModelingTool(m_activeTool.get(), ctx);   // peut appeler applyActiveModelingTool
        return;
    }

    TSA::UI::ModelingToolDialog dlg(*m_activeTool, this);
    m_toolDialogRunning = true;
    syncToolActionStates();   // bouton coloré pendant la saisie dans la fenêtre
    const bool accepted = dlg.exec() == QDialog::Accepted;
    m_toolDialogRunning = false;
    if (accepted) applyActiveModelingTool();
    m_activeTool.reset();
}

void MainWindow::applyActiveModelingTool()
{
    if (!m_model || !m_activeTool) return;
    const bool fromViewport = m_occView && m_occView->activeModelingTool() == m_activeTool.get();
    const TSA::Interaction::ToolContext ctx = modelingToolContext();
    const QString name = QString::fromStdString(m_activeTool->name());

    if (m_activeTool->category() == TSA::Interaction::ToolCategory::Annotate)
    {
        // Cotation : annotation ajoutée par Annotation::addDimension (sa propre entrée Annuler, révision
        // inchangée : les résultats de calcul restent valides). Pas de transaction géométrique.
        const TSA::Interaction::ToolResult result = m_activeTool->apply(*m_model, ctx);
        const QString message = QString::fromStdString(result.message);
        if (m_consoleDock) m_consoleDock->appendLog(QStringLiteral("%1 : %2").arg(name, message), result.success ? "INFO" : "WARN");
        if (m_statusInfo) m_statusInfo->setText(QStringLiteral("%1 : %2").arg(name, message));
        updateUndoRedoActions();
        updateWindowTitle();
        if (fromViewport) m_occView->modelingToolApplied(true);
        return;
    }

    TSA::UndoRedo::EditTransaction tx(*m_model, m_activeTool->name());
    const TSA::Interaction::ToolResult result = m_activeTool->apply(*m_model, ctx);
    const QString message = QString::fromStdString(result.message);
    if (!result.success)
    {
        tx.rollback();
    }
    else
    {
        tx.record({ "modify", "Selection", -1, m_activeTool->id(), "", result.message, { "geometry", "results_invalidated" } });
        tx.commit();
        if (m_selectionManager && !result.created.empty()) m_selectionManager->selectElements(result.created);
        if (m_modelTree) m_modelTree->refreshAll();
        if (m_occView) m_occView->update();
    }

    if (m_consoleDock) m_consoleDock->appendLog(QStringLiteral("%1 : %2").arg(name, message), result.success ? "INFO" : "WARN");
    if (m_statusInfo) m_statusInfo->setText(QStringLiteral("%1 : %2").arg(name, message));
    updateUndoRedoActions();

    if (fromViewport)
    {
        const bool keep = m_activeTool->continuesAfterApply() || !result.success;
        if (keep && m_selectionManager) m_occView->setModelingToolSelection(m_selectionManager->selectedElements());
        m_occView->modelingToolApplied(keep);
        if (!keep) m_activeTool.reset();
    }
}
