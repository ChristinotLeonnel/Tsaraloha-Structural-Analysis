// Raccourcis clavier de la fenêtre principale : chaque action existante est reliée à son identifiant du
// catalogue (src/Commands/CommandCatalog.cpp) ; ShortcutManager pose les raccourcis (valeurs par défaut,
// puis fichier utilisateur shortcut.txt rechargé à chaud). Aucune logique métier ici.

#include "MainWindow.h"

#include "Dialogs/HelpDialog.h"
#include "Dialogs/ShortcutEditorDialog.h"
#include "Dock/LogConsoleDock.h"
#include "Dock/ResultsDockWidget.h"
#include "Ruler/ViewportContainer.h"
#include "Shortcuts/ShortcutManager.h"
#include "WindowManager/WindowItem.h"
#include "WindowManager/WindowManager.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QLabel>

using TSA::UI::Shortcuts::ShortcutManager;

void MainWindow::bindShortcut(const char* id, QAction* action)
{
    if (!action) return;
    ShortcutManager::instance().bind(QString::fromLatin1(id), action);
    // Associée à la fenêtre : le raccourci reste actif même si l'action n'est visible que dans un onglet
    // du ruban masqué (QWidget::addAction ignore les doublons).
    addAction(action);
}

void MainWindow::setupShortcuts()
{
    const std::pair<const char*, QAction*> table[] = {
        // Gestion des projets
        { "cmd.file.new", m_actionNew }, { "cmd.file.open", m_actionOpen }, { "cmd.file.save", m_actionSave },
        { "cmd.file.save_as", m_actionSaveAs }, { "cmd.file.close", m_actionCloseProject }, { "cmd.file.exit", m_actionExit },
        { "cmd.file.export_diagnostic", m_actionExportDiagnostic }, { "cmd.model.topology", m_actionTopology },
        { "cmd.model.clean", m_actionCleanModel },
        // Édition
        { "cmd.edit.undo", m_actionUndo }, { "cmd.edit.redo", m_actionRedo }, { "cmd.edit.copy", m_actionCopyClipboard },
        { "cmd.edit.paste", m_actionPasteClipboard }, { "cmd.edit.delete", m_actionDelete },
        { "cmd.edit.repeat", ShortcutManager::instance().repeatAction() },
        // Sélection
        { "cmd.select.mode", m_actionSelectMode }, { "cmd.select.all", m_actionSelectAll },
        // Modélisation
        { "cmd.create.node", m_actionDrawNode }, { "cmd.create.node_dialog", m_actionNewNode }, { "cmd.create.beam", m_actionDrawBeam },
        { "cmd.create.column", m_actionDrawColumn }, { "cmd.create.bar", m_actionDrawBar }, { "cmd.create.cable", m_actionDrawCable },
        { "cmd.create.slab", m_actionDrawSlab }, { "cmd.create.wall", m_actionDrawWall }, { "cmd.create.truss", m_actionTruss },
        { "cmd.create.foundation", m_actionFooting }, { "cmd.create.cube", m_actionAddCube }, { "cmd.create.presets", m_actionStructurePresets },
        { "cmd.support.fixed", m_actionFixed }, { "cmd.support.pinned", m_actionPinned }, { "cmd.support.roller", m_actionRoller },
        // Modification
        { "cmd.modify.move", m_actionMove3D }, { "cmd.modify.copy_3d", m_actionCopy3D }, { "cmd.modify.copy", m_actionCopy },
        { "cmd.modify.translate", m_actionMove }, { "cmd.modify.rotate", m_actionRotate3D }, { "cmd.modify.mirror", m_actionMirror },
        { "cmd.modify.split_bars", m_actionSplitBars }, { "cmd.modify.merge_nodes", m_actionMergeNodes },
        { "cmd.modify.move_origin", m_actionMoveOrigin }, { "cmd.modify.viewport_input", m_actionToolInputViewport },
        // Cotation et mesure
        { "cmd.dim.edit", m_actionEditDimension }, { "cmd.dim.delete", m_actionDeleteDimensions },
        { "cmd.dim.visible", m_actionDimensionsVisible }, { "cmd.dim.style", m_actionDimensionStyle },
        { "cmd.dim.clean", m_actionCleanDimensions }, { "cmd.tools.measure", m_actionMeasure },
        // Grille, accrochage et plans de travail
        { "cmd.display.grid", m_actionGridVisible }, { "cmd.snap.grid", m_actionGridSnap }, { "cmd.snap.object_snap", m_actionObjectSnap },
        { "cmd.display.levels", m_actionLevelsVisible }, { "cmd.display.grid_labels", m_actionGridLabels },
        { "cmd.display.rulers", m_actionRulersVisible }, { "cmd.coord.workplane_xy", m_actionWorkPlaneXY },
        { "cmd.coord.workplane_xz", m_actionWorkPlaneXZ }, { "cmd.coord.workplane_yz", m_actionWorkPlaneYZ },
        { "cmd.coord.workplane_level", m_actionWorkPlaneLevel }, { "cmd.coord.workplane_custom", m_actionWorkPlaneCustom },
        { "cmd.coord.workplane_visible", m_actionWorkPlaneVisible }, { "cmd.view.normal_to_plane", m_actionViewNormalToPlane },
        { "cmd.struct.levels", m_actionManageLevels }, { "cmd.struct.grid_dialog", m_actionGridManager },
        { "cmd.struct.new_grid", m_actionNewGrid },
        // Navigation
        { "cmd.view.fit_all", m_actionFitAll }, { "cmd.view.fit_selection", m_actionFitSelection }, { "cmd.view.zoom_in", m_actionZoomIn },
        { "cmd.view.zoom_out", m_actionZoomOut }, { "cmd.view.zoom_window", m_actionZoomWindow }, { "cmd.view.prev", m_actionPreviousView },
        { "cmd.view.next", m_actionNextView }, { "cmd.view.home", m_actionViewHome }, { "cmd.view.reset", m_actionResetView },
        { "cmd.view.top", m_actionViewTop }, { "cmd.view.bottom", m_actionViewBottom }, { "cmd.view.front", m_actionViewFront },
        { "cmd.view.back", m_actionViewBack }, { "cmd.view.left", m_actionViewLeft }, { "cmd.view.right", m_actionViewRight },
        { "cmd.view.iso", m_actionViewIsometric }, { "cmd.view.3d", m_actionView3D }, { "cmd.view.xy", m_actionViewXY },
        { "cmd.view.xz", m_actionViewXZ }, { "cmd.view.yz", m_actionViewYZ }, { "cmd.view.rotate_left", m_actionRotateLeft },
        { "cmd.view.rotate_right", m_actionRotateRight }, { "cmd.view.section_cut", m_actionSectionCut },
        { "cmd.view.coord_system", m_actionCoordSystem }, { "cmd.view.fullscreen", m_actionFullScreen },
        // Affichage
        { "cmd.display.nodes", m_actionNodesVisible }, { "cmd.display.node_labels", m_actionNodeLabelsVisible },
        { "cmd.display.supports", m_actionSupportsVisible }, { "cmd.display.support_labels", m_actionSupportLabelsVisible },
        { "cmd.display.loads", m_actionLoadsVisible }, { "cmd.display.forces", m_actionForcesVisible },
        { "cmd.display.moments", m_actionMomentsVisible }, { "cmd.display.load_values", m_actionLoadValuesVisible },
        // Propriétés, matériaux et sections
        { "cmd.properties.panel", m_propertiesDock ? m_propertiesDock->toggleViewAction() : nullptr },
        { "cmd.library.custom", m_actionLibrary }, { "cmd.library.manager", m_actionExtensionManager },
        { "cmd.section.i", m_actionSecI }, { "cmd.section.rect", m_actionSecRect }, { "cmd.section.circ", m_actionSecCirc },
        { "cmd.material.concrete", m_actionConcrete }, { "cmd.material.steel", m_actionSteel },
        // Charges
        { "cmd.loads.point", m_actionPointLoad }, { "cmd.loads.distributed", m_actionDistLoad },
        { "cmd.loads.trapezoidal", m_actionTrapLoad }, { "cmd.loads.bar_point", m_actionBarPointLoad },
        { "cmd.loads.surface", m_actionSurfaceLoad }, { "cmd.loads.self_weight", m_actionSelfWeight },
        { "cmd.loadcases.manager", m_actionLoadCases },
        // Maillage, analyse, résultats, rapports, BIM
        { "cmd.analysis.mesh", m_actionMeshGen }, { "cmd.analysis.solve", m_actionRunSolve }, { "cmd.analysis.config", m_actionAnalysisConfig },
        { "cmd.results.deformed", m_actionDeformedToggle }, { "cmd.results.reactions", m_actionReactionsToggle },
        { "cmd.results.diagram_mz", m_actionDiagramMz }, { "cmd.results.diagram_my", m_actionDiagramMy },
        { "cmd.results.diagram_mx", m_actionDiagramMx }, { "cmd.results.diagram_vz", m_actionDiagramVz },
        { "cmd.results.diagram_vy", m_actionDiagramVy }, { "cmd.results.diagram_n", m_actionDiagramN },
        { "cmd.results.diagram_deflection", m_actionDiagramDeflection }, { "cmd.results.diagram_none", m_actionDiagramNone },
        { "cmd.results.fit_model", m_actionFitModel }, { "cmd.results.fit_results", m_actionFitResults },
        { "cmd.results.fit_deformed", m_actionFitDeformed }, { "cmd.results.displacements", m_actionResultsDisp },
        { "cmd.results.forces", m_actionResultsForces }, { "cmd.results.stresses", m_actionResultsStress },
        { "cmd.report.ndc", m_actionNoteDeCalcul },
        { "cmd.report.templates", m_actionReportTemplates }, { "cmd.bim.import_ifc", m_actionImportIfc }, { "cmd.bim.export_ifc", m_actionExportIfc },
        { "cmd.bim.import_tsa3d", m_actionImportTsa3d }, { "cmd.bim.export_tsa3d", m_actionExportTsa3d },
        { "cmd.bim.import_module", m_actionImportViaModule },
        // Fenêtres
        { "cmd.window.results", m_resultsDock ? m_resultsDock->toggleViewAction() : nullptr },
        // Outils, paramètres, aide
        { "cmd.ai.assistant", m_actionAIAssistant }, { "cmd.ai.config", m_actionAIConfig }, { "cmd.ai.check", m_actionAICheck },
        { "cmd.ai.analyze", m_actionAIAnalyze }, { "cmd.ai.explain", m_actionAIExplain }, { "cmd.settings.theme", m_actionToggleTheme },
        { "cmd.help.shortcuts", m_actionShortcuts }, { "cmd.help.shortcut_editor", m_actionShortcutEditor },
        { "cmd.help.modules", m_actionModules },
        { "cmd.help.full", m_actionHelp }, { "cmd.help.online_docs", m_actionOnlineDocs },
        { "cmd.help.report_problem", m_actionReportProblem }, { "cmd.help.about", m_actionAbout },
    };
    for (const auto& [id, action] : table) bindShortcut(id, action);

    // Outils du registre (modification, dessin, cotation) : cmd.tool.<identifiant de l'outil>
    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it)
    {
        const QByteArray id = "cmd.tool." + QByteArray::fromStdString(it->first);
        bindShortcut(id.constData(), it->second);
    }
    // Représentations du modèle (Affichage > Représentation)
    const char* displayIds[] = { "cmd.view.display_physical", "cmd.view.display_analytical", "cmd.view.display_fe", "cmd.view.display_overlay" };
    for (std::size_t i = 0; i < m_displayModeActions.size() && i < 4; ++i) bindShortcut(displayIds[i], m_displayModeActions[i]);
    // Panneaux (actions du gestionnaire de fenêtres)
    if (m_windowManager)
        for (const char* dock : { "model_browser", "visibility", "elements", "properties", "work_planes", "analysis_data", "console" })
            if (const auto* info = m_windowManager->findWindow(QString::fromLatin1(dock)); info && info->action)
                bindShortcut(("cmd.window." + QByteArray(dock)).constData(), info->action);
    // Boutons rapides du bandeau de la vue (Z / X / Y)
    if (m_viewportContainer)
    {
        const char* axisIds[] = { "cmd.coord.axis_z", "cmd.coord.axis_x", "cmd.coord.axis_y" };
        const auto axisActions = m_viewportContainer->quickAxisActions();
        for (int i = 0; i < axisActions.size() && i < 3; ++i) bindShortcut(axisIds[i], axisActions[i]);
    }

    // Champs de saisie : leurs touches ne déclenchent pas de commande.
    qApp->installEventFilter(new TSA::UI::Shortcuts::ShortcutInputGuard(this));

    auto& mgr = ShortcutManager::instance();
    connect(&mgr, &ShortcutManager::configApplied, this, [this](const QString& summary, const QStringList& warnings) {
        if (m_consoleDock)
        {
            m_consoleDock->appendLog(tr("Raccourcis clavier : %1").arg(summary), "INFO");
            for (const auto& w : warnings) m_consoleDock->appendLog(tr("Raccourcis clavier : %1").arg(w), "WARN");
        }
        if (m_statusInfo) m_statusInfo->setText(tr("Raccourcis appliqués : %1").arg(summary));
    });
    connect(&mgr, &ShortcutManager::configRejected, this, [this](const QStringList& errors) {
        if (m_consoleDock)
            for (const auto& e : errors) m_consoleDock->appendLog(tr("Raccourcis clavier : %1").arg(e), "ERROR");
        if (m_statusInfo)
            m_statusInfo->setText(tr("shortcut.txt refusé (%1 erreur(s), voir la console) : configuration précédente conservée")
                                      .arg(std::max<qsizetype>(1, errors.size() - 1)));
    });
    connect(&mgr, &ShortcutManager::shortcutsChanged, this, [this] {
        if (m_viewportContainer) m_viewportContainer->syncQuickAxisToolTips();
    });

    if (mgr.configPath().isEmpty()) mgr.setConfigPath(ShortcutManager::defaultConfigPath());
    mgr.loadConfig();
    if (m_viewportContainer) m_viewportContainer->syncQuickAxisToolTips();
}

void MainWindow::onActionShortcutEditor()
{
    TSA::UI::ShortcutEditorDialog dlg(ShortcutManager::instance(), this);
    dlg.exec();
}
