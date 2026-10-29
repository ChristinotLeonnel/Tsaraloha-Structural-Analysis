#include "RibbonBuilder.h"
#include "RibbonBar.h"
#include "RibbonTab.h"
#include "RibbonPanel.h"
#include "RibbonButton.h"

#include "UI/Theme/ThemedIcon.h"

#include <QAction>
#include <QMenu>

#include <algorithm>
#include <set>

namespace TSA::UI
{

namespace
{
std::vector<QAction*> live(std::initializer_list<QAction*> actions)
{
    std::vector<QAction*> out;
    for (QAction* a : actions)
        if (a) out.push_back(a);
    return out;
}

/// Groupe d'un onglet : grands boutons (commandes principales) puis colonnes de petits boutons (variantes, options).
class PanelBuilder
{
public:
    PanelBuilder(RibbonTab* tab, const QString& title) : m_tab(tab), m_panel(new RibbonPanel(title, tab)) {}
    ~PanelBuilder()
    {
        // Jamais de groupe vide : une fenêtre sans ces commandes n'affiche pas le groupe
        if (m_panel->isEmptyPanel()) delete m_panel;
        else m_tab->addPanel(m_panel);
    }

    PanelBuilder& large(std::initializer_list<QAction*> actions)
    {
        for (QAction* a : live(actions))
        {
            separate();
            m_panel->addLargeAction(a);
            m_items = true;
            m_lastLarge = true;
        }
        return *this;
    }

    /// Petits boutons, trois par colonne ; un séparateur avant la première colonne.
    PanelBuilder& small(const std::vector<QAction*>& actions)
    {
        if (actions.empty()) return *this;
        if (m_items) m_panel->addInternalSeparator();
        for (std::size_t i = 0; i < actions.size(); i += 3)
            m_panel->addSmallColumn(std::vector<QAction*>(actions.begin() + i, actions.begin() + std::min(i + 3, actions.size())));
        m_items = true;
        m_lastLarge = false;
        return *this;
    }
    PanelBuilder& small(std::initializer_list<QAction*> actions) { return small(live(actions)); }

    PanelBuilder& menu(const QString& text, const QString& icon, QMenu* menu)
    {
        if (!menu) return *this;
        if (m_items) m_panel->addInternalSeparator();
        m_panel->addMenuButton(text, themedIcon(icon), menu, RibbonButtonSize::Small);
        m_items = true;
        return *this;
    }

private:
    void separate()
    {
        if (m_items && !m_lastLarge) m_panel->addInternalSeparator();
    }

    RibbonTab* m_tab;
    RibbonPanel* m_panel;
    bool m_items = false;
    bool m_lastLarge = false;
};
} // namespace

void RibbonBuilder::buildAllTabs(RibbonBar* bar, const RibbonActions& acts, QWidget* /*parentWindow*/)
{
    if (!bar) return;

    // Barre d'accès rapide : les commandes de tous les jours, quel que soit l'onglet actif.
    // Nouveau, Ouvrir, Enregistrer, Annuler et Rétablir sont dans la barre de titre (AppShell).
    // Les trois dispositions de l'espace de travail y sont toujours atteignables, quel que soit l'onglet.
    bar->setQuickAccess({ acts.actionSelectMode, acts.actionFitAll, acts.actionView3D, acts.actionRunSolve,
                          acts.actionWorkspace3D, acts.actionWorkspaceBlueprint, acts.actionWorkspaceSplit });

    // Ordre du travail : projet → modèle → modification → programmation (Blueprint) → repères → charges → maillage → calcul
    // → résultats → vues → outils.
    buildHomeTab(bar, acts);
    buildModelingTab(bar, acts);
    buildModifyTab(bar, acts);
    buildBlueprintTab(bar, acts);
    buildGridTab(bar, acts);
    buildLoadsTab(bar, acts);
    buildMeshTab(bar, acts);
    buildAnalysisTab(bar, acts);
    buildResultsTab(bar, acts);
    buildViewTab(bar, acts);
    buildToolsTab(bar, acts);
}

// -----------------------------------------------------------------------------
// 1. Accueil : projet, historique, échanges, paramètres du projet, préférences
// -----------------------------------------------------------------------------
void RibbonBuilder::buildHomeTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Accueil"));
    PanelBuilder(tab, tr("Projet")).large({ a.actionNew }).small({ a.actionOpen, a.actionSave, a.actionSaveAs }).large({ a.actionCloseProject });
    {
        PanelBuilder p(tab, tr("Historique et presse-papiers"));
        p.small({ a.actionUndo, a.actionRedo }).small({ a.actionCutClipboard, a.actionCopyClipboard }).small({ a.actionPasteClipboard, a.actionPasteOriginal });
    }
    PanelBuilder(tab, tr("Échanges BIM")).large({ a.actionImportIfc })
        .small({ a.actionExportIfc, a.actionImportTsa3d, a.actionExportTsa3d, a.actionImportViaModule, a.actionBimGeometryVisible });
    PanelBuilder(tab, tr("Paramètres du projet")).large({ a.actionTopology }).small({ a.actionStructurePresets });
    PanelBuilder(tab, tr("Préférences")).large({ a.actionLanguage }).small({ a.actionToggleTheme, a.actionShortcutEditor });
}

// -----------------------------------------------------------------------------
// 2. Modélisation : sélection, éléments, sections et matériaux, appuis
// -----------------------------------------------------------------------------
void RibbonBuilder::buildModelingTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Modélisation"));
    PanelBuilder(tab, tr("Sélection")).large({ a.actionSelectMode }).small({ a.actionSelectAll });
    PanelBuilder(tab, tr("Éléments filaires")).large({ a.actionDrawBeam, a.actionDrawColumn }).small({ a.actionDrawBar, a.actionDrawCable, a.actionTruss });
    PanelBuilder(tab, tr("Surfaces")).large({ a.actionDrawSlab, a.actionDrawWall }).small({ a.actionFooting, a.actionSlabOpenings });
    PanelBuilder(tab, tr("Nœuds")).large({ a.actionDrawNode }).small({ a.actionNewNode, a.actionAddCube });
    if (!a.drawTools.empty()) PanelBuilder(tab, tr("Dessin rapide")).small(a.drawTools);
    PanelBuilder(tab, tr("Sections et matériaux")).large({ a.actionSecI }).small({ a.actionSecRect, a.actionSecCirc, a.actionLibrary })
        .small({ a.actionConcrete, a.actionSteel });
    PanelBuilder(tab, tr("Appuis")).large({ a.actionFixed }).small({ a.actionPinned, a.actionRoller, a.actionSurfaceSupports });
}

// -----------------------------------------------------------------------------
// 3. Modifier : transformations, topologie, outils de modification, suppression
// -----------------------------------------------------------------------------
void RibbonBuilder::buildModifyTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Modifier"));
    PanelBuilder(tab, tr("Déplacer et copier")).large({ a.actionMove3D, a.actionCopy3D }).small({ a.actionMove, a.actionCopy, a.actionRotate3D });
    PanelBuilder(tab, tr("Symétrie et topologie")).large({ a.actionMirror }).small({ a.actionSplitBars, a.actionMergeNodes, a.actionCleanModel });
    PanelBuilder(tab, tr("Outils de modification")).large({ a.actionToolInputMode }).small(a.advancedModifyTools);
    PanelBuilder(tab, tr("Suppression")).large({ a.actionDelete });
}

// -----------------------------------------------------------------------------
// 3 bis. Blueprint : programmes de modélisation en blocs (éditeur dans l'espace de travail, même projet)
// Les commandes du graphe (copier, sous-graphes, débogage pas à pas…) restent dans la barre de l'éditeur, au contact du
// graphe ; le ruban porte l'accès, le fichier, l'exécution et la navigation.
// -----------------------------------------------------------------------------
void RibbonBuilder::buildBlueprintTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Blueprint"));
    PanelBuilder(tab, tr("Fichier")).large({ a.actionBlueprintNew }).small({ a.actionBlueprintOpen, a.actionBlueprintSave });
    PanelBuilder(tab, tr("Exécution")).large({ a.actionBlueprintRun }).small({ a.actionBlueprintValidate, a.actionBlueprintDebug });
    PanelBuilder(tab, tr("Navigation du graphe")).large({ a.actionBlueprintFitAll }).small({ a.actionBlueprintFitSelection });
}

// -----------------------------------------------------------------------------
// 4. Grille et plans : grilles, niveaux, plan de travail et sa navigation, accrochage, aides, repère, coupes
// -----------------------------------------------------------------------------
void RibbonBuilder::buildGridTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Grille et plans"));
    PanelBuilder(tab, tr("Grilles et niveaux")).large({ a.actionNewGrid }).small({ a.actionGridManager, a.actionManageLevels });
    PanelBuilder(tab, tr("Plan de travail")).large({ a.actionWorkPlaneXY })
        .small({ a.actionWorkPlaneXZ, a.actionWorkPlaneYZ, a.actionWorkPlaneLevel })
        .small({ a.actionWorkPlaneCustom, a.actionWorkPlaneVisible, a.actionViewNormalToPlane });
    PanelBuilder(tab, tr("Navigation sur la grille"))
        .small({ a.actionWorkPlaneNextRef, a.actionWorkPlanePrevRef, a.actionWorkPlaneBack })
        .small({ a.actionWorkPlaneNextInter, a.actionWorkPlanePrevInter, a.actionWorkPlaneAlignInter });
    PanelBuilder(tab, tr("Accrochage")).small({ a.actionGridSnap, a.actionObjectSnap });
    PanelBuilder(tab, tr("Aides visuelles")).small({ a.actionGridVisible, a.actionLevelsVisible, a.actionGridLabels })
        .small({ a.actionRulersVisible })
        .menu(tr("Échelle des annotations"), QStringLiteral(":/icons/grid_labels.svg"), a.annotationScaleMenu);
    PanelBuilder(tab, tr("Repère et coupes")).large({ a.actionCoordSystem, a.actionMoveOrigin, a.actionSectionCut });
}

// -----------------------------------------------------------------------------
// 5. Charges : par support (nœud, barre, surface), poids propre, cas et combinaisons, affichage
// -----------------------------------------------------------------------------
void RibbonBuilder::buildLoadsTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Charges"));
    PanelBuilder(tab, tr("Nœud")).large({ a.actionPointLoad });
    PanelBuilder(tab, tr("Barre")).large({ a.actionDistLoad, a.actionTrapLoad, a.actionBarPointLoad });
    PanelBuilder(tab, tr("Surface")).large({ a.actionSurfaceLoad });
    PanelBuilder(tab, tr("Poids propre")).large({ a.actionSelfWeight });
    PanelBuilder(tab, tr("Cas et combinaisons")).large({ a.actionLoadCases });
    PanelBuilder(tab, tr("Affichage des charges")).large({ a.actionLoadsVisible })
        .small({ a.actionForcesVisible, a.actionMomentsVisible, a.actionLoadValuesVisible })
        .small({ a.actionSurfaceLoadsVisible })
        .menu(tr("Échelle des charges"), QStringLiteral(":/icons/load_point.svg"), a.loadScaleMenu);
}

// -----------------------------------------------------------------------------
// 6. Maillage : génération, paramètres, contrôle, affichage, échanges
// -----------------------------------------------------------------------------
void RibbonBuilder::buildMeshTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Maillage"));
    PanelBuilder(tab, tr("Génération")).large({ a.actionMeshGen }).small({ a.actionMeshRegenerate, a.actionMeshClear });
    PanelBuilder(tab, tr("Paramètres")).large({ a.actionMeshSettings }).small({ a.actionMeshLocal, a.actionMeshZone });
    PanelBuilder(tab, tr("Contrôle")).small({ a.actionMeshPreview, a.actionMeshQuality });
    PanelBuilder(tab, tr("Affichage")).large({ a.actionMeshShow })
        .menu(tr("Affichage du maillage"), QStringLiteral(":/icons/analysis/mesh_show.svg"), a.meshViewMenu);
    PanelBuilder(tab, tr("Échanges")).small({ a.actionMeshExport, a.actionMeshImport });
}

// -----------------------------------------------------------------------------
// 7. Analyse : calcul et ses paramètres, vérification
// -----------------------------------------------------------------------------
void RibbonBuilder::buildAnalysisTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Analyse"));
    PanelBuilder(tab, tr("Calcul")).large({ a.actionRunSolve, a.actionAnalysisConfig });
    PanelBuilder(tab, tr("Vérification (IA)")).large({ a.actionAICheck }).small({ a.actionAIAnalyze });
}

// -----------------------------------------------------------------------------
// 8. Résultats : panneau, déformée, diagrammes, réactions, cadrage, rapports
// -----------------------------------------------------------------------------
void RibbonBuilder::buildResultsTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Résultats"));
    PanelBuilder(tab, tr("Panneau")).large({ a.actionResultsDock });
    PanelBuilder(tab, tr("Déformée et efforts")).large({ a.actionDeformedToggle })
        .small({ a.actionResultsDisp, a.actionResultsForces, a.actionResultsStress });
    PanelBuilder(tab, tr("Diagrammes")).small({ a.actionDiagramMz, a.actionDiagramMy, a.actionDiagramMx })
        .small({ a.actionDiagramVz, a.actionDiagramVy, a.actionDiagramN })
        .small({ a.actionDiagramDeflection, a.actionDiagramNone });
    PanelBuilder(tab, tr("Réactions")).large({ a.actionReactionsToggle });
    PanelBuilder(tab, tr("Cadrage")).large({ a.actionFitDeformed }).small({ a.actionFitResults, a.actionFitModel });
    PanelBuilder(tab, tr("Rapports")).large({ a.actionOpenNDC }).small({ a.actionReportTemplates, a.actionAIExplain });
}

// -----------------------------------------------------------------------------
// 9. Affichage : vues, navigation, représentation, rendu, visibilité, fenêtres
// -----------------------------------------------------------------------------
void RibbonBuilder::buildViewTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Affichage"));
    PanelBuilder(tab, tr("Vues")).large({ a.actionView3D })
        .small({ a.actionViewXY, a.actionViewXZ, a.actionViewYZ })
        .small({ a.actionViewTop, a.actionViewBottom, a.actionViewFront, a.actionViewBack, a.actionViewLeft, a.actionViewRight })
        .small({ a.actionViewIsometric, a.actionViewHome, a.actionNamedViews });
    PanelBuilder(tab, tr("Navigation")).large({ a.actionFitAll })
        .small({ a.actionFitSelection, a.actionZoomWindow, a.actionResetView })
        .small({ a.actionZoomIn, a.actionZoomOut, a.actionPreviousView, a.actionNextView, a.actionRotateLeft, a.actionRotateRight });
    {
        // Deux premiers modes en grand (physique, analytique), les autres en colonne : aucun mode laissé de côté
        const auto& modes = a.modelDisplayModes;
        PanelBuilder p(tab, tr("Représentation"));
        for (std::size_t i = 0; i < std::min<std::size_t>(2, modes.size()); ++i) p.large({ modes[i] });
        if (modes.size() > 2) p.small(std::vector<QAction*>(modes.begin() + 2, modes.end()));
    }
    {
        PanelBuilder p(tab, tr("Rendu"));
        p.small(a.renderModes).small({ a.renderEdges, a.renderLightweight });
    }
    PanelBuilder(tab, tr("Visibilité")).small({ a.actionNodesVisible, a.actionNodeLabelsVisible, a.actionSupportsVisible })
        .small({ a.actionSupportLabelsVisible, a.actionFullScreen });
    PanelBuilder(tab, tr("Espace de travail")).large({ a.actionWorkspace3D, a.actionWorkspaceBlueprint, a.actionWorkspaceSplit })
        .small({ a.actionWorkspaceVertical, a.actionFocusMode, a.actionResetLayout });
    PanelBuilder(tab, tr("Fenêtres")).small({ a.actionToggleModelTree, a.actionToggleProperties })
        .small({ a.actionToggleVisibility, a.actionToggleConsole });
}

// -----------------------------------------------------------------------------
// 10. Outils : mesure, cotation, assistant IA, extensions, aide
// -----------------------------------------------------------------------------
void RibbonBuilder::buildToolsTab(RibbonBar* bar, const RibbonActions& a)
{
    RibbonTab* tab = bar->addTab(tr("Outils"));
    PanelBuilder(tab, tr("Mesure")).large({ a.actionMeasure });
    if (!a.dimensionTools.empty() || !a.dimensionCommands.empty())
    {
        auto tool = [&](const char* id) -> QAction* {
            auto it = a.dimensionTools.find(id);
            return it != a.dimensionTools.end() ? it->second : nullptr;
        };
        PanelBuilder dim(tab, tr("Cotation"));
        dim.large({ tool("dim_aligned"), tool("dim_linear") })
            .small({ tool("dim_x"), tool("dim_y"), tool("dim_z") })
            .small({ tool("dim_horizontal"), tool("dim_angular"), tool("dim_level") })
            .small({ tool("dim_chain"), tool("dim_cumulative") });
        // Outils de cotation ajoutés au registre après cette liste : jamais perdus
        std::vector<QAction*> others;
        static const std::set<std::string> known = { "dim_aligned", "dim_linear", "dim_x", "dim_y", "dim_z", "dim_horizontal",
                                                     "dim_angular", "dim_level", "dim_chain", "dim_cumulative" };
        for (const auto& [id, action] : a.dimensionTools)
            if (action && !known.count(id)) others.push_back(action);
        std::vector<QAction*> commands;
        for (QAction* c : a.dimensionCommands)
            if (c) commands.push_back(c);
        dim.small(others).small(commands);
    }
    PanelBuilder(tab, tr("Assistant IA")).large({ a.actionAIAssistant }).small({ a.actionAIConfig });
    PanelBuilder(tab, tr("Extensions")).large({ a.actionExtensionManager }).small({ a.actionModules });
    PanelBuilder(tab, tr("Aide")).large({ a.actionHelp })
        .small({ a.actionOnlineDocs, a.actionShortcuts, a.actionAbout })
        .small({ a.actionReportProblem, a.actionExportDiagnostic });
}

std::vector<QAction*> RibbonBuilder::placedActions(const RibbonActions& a)
{
    std::vector<QAction*> all = {
        a.actionNew, a.actionCloseProject, a.actionOpen, a.actionSave, a.actionSaveAs,
        a.actionUndo, a.actionRedo, a.actionCopyClipboard, a.actionPasteClipboard, a.actionCutClipboard, a.actionPasteOriginal,
        a.actionImportIfc, a.actionExportIfc, a.actionImportTsa3d, a.actionExportTsa3d, a.actionImportViaModule,
        a.actionBimGeometryVisible, a.actionTopology, a.actionStructurePresets, a.actionLanguage, a.actionToggleTheme,
        a.actionShortcutEditor,
        a.actionSelectMode, a.actionSelectAll, a.actionDrawNode, a.actionNewNode, a.actionDrawBar, a.actionDrawBeam,
        a.actionDrawColumn, a.actionDrawCable, a.actionDrawSlab, a.actionDrawWall, a.actionTruss, a.actionFooting,
        a.actionSlabOpenings, a.actionAddCube, a.actionSecI, a.actionSecRect, a.actionSecCirc, a.actionConcrete, a.actionSteel,
        a.actionLibrary, a.actionFixed, a.actionPinned, a.actionRoller, a.actionSurfaceSupports,
        a.actionMove3D, a.actionMove, a.actionCopy3D, a.actionCopy, a.actionRotate3D, a.actionMirror, a.actionSplitBars,
        a.actionMergeNodes, a.actionCleanModel, a.actionToolInputMode, a.actionDelete,
        a.actionNewGrid, a.actionGridManager, a.actionManageLevels, a.actionWorkPlaneXY, a.actionWorkPlaneXZ,
        a.actionWorkPlaneYZ, a.actionWorkPlaneLevel, a.actionWorkPlaneCustom, a.actionWorkPlaneVisible,
        a.actionViewNormalToPlane, a.actionWorkPlaneNextRef, a.actionWorkPlanePrevRef, a.actionWorkPlaneBack,
        a.actionWorkPlaneNextInter, a.actionWorkPlanePrevInter, a.actionWorkPlaneAlignInter, a.actionGridSnap,
        a.actionObjectSnap, a.actionGridVisible, a.actionLevelsVisible, a.actionGridLabels, a.actionRulersVisible,
        a.actionCoordSystem, a.actionMoveOrigin, a.actionSectionCut,
        a.actionPointLoad, a.actionDistLoad, a.actionTrapLoad, a.actionBarPointLoad, a.actionSurfaceLoad, a.actionSelfWeight,
        a.actionLoadCases, a.actionLoadsVisible, a.actionForcesVisible, a.actionMomentsVisible, a.actionLoadValuesVisible,
        a.actionSurfaceLoadsVisible,
        a.actionMeshGen, a.actionMeshRegenerate, a.actionMeshClear, a.actionMeshSettings, a.actionMeshLocal, a.actionMeshZone,
        a.actionMeshPreview, a.actionMeshQuality, a.actionMeshShow, a.actionMeshExport, a.actionMeshImport,
        a.actionRunSolve, a.actionAnalysisConfig, a.actionAICheck, a.actionAIAnalyze,
        a.actionResultsDock, a.actionDeformedToggle, a.actionResultsDisp, a.actionResultsForces, a.actionResultsStress,
        a.actionDiagramMz, a.actionDiagramMy, a.actionDiagramMx, a.actionDiagramVz, a.actionDiagramVy, a.actionDiagramN,
        a.actionDiagramDeflection, a.actionDiagramNone, a.actionReactionsToggle, a.actionFitModel, a.actionFitResults,
        a.actionFitDeformed, a.actionOpenNDC, a.actionReportTemplates, a.actionAIExplain,
        a.actionView3D, a.actionViewXY, a.actionViewXZ, a.actionViewYZ, a.actionViewTop, a.actionViewBottom,
        a.actionViewFront, a.actionViewBack, a.actionViewLeft, a.actionViewRight, a.actionViewIsometric, a.actionViewHome,
        a.actionNamedViews, a.actionFitAll, a.actionFitSelection, a.actionResetView, a.actionZoomIn, a.actionZoomOut,
        a.actionZoomWindow, a.actionPreviousView, a.actionNextView, a.actionRotateLeft, a.actionRotateRight,
        a.renderEdges, a.renderLightweight, a.actionNodesVisible, a.actionNodeLabelsVisible, a.actionSupportsVisible,
        a.actionSupportLabelsVisible, a.actionFullScreen, a.actionToggleModelTree, a.actionToggleProperties,
        a.actionToggleVisibility, a.actionToggleConsole,
        a.actionWorkspace3D, a.actionWorkspaceBlueprint, a.actionWorkspaceSplit, a.actionWorkspaceVertical, a.actionFocusMode,
        a.actionResetLayout, a.actionBlueprintNew, a.actionBlueprintOpen, a.actionBlueprintSave, a.actionBlueprintValidate,
        a.actionBlueprintRun, a.actionBlueprintDebug, a.actionBlueprintFitAll, a.actionBlueprintFitSelection,
        a.actionMeasure, a.actionAIAssistant, a.actionAIConfig, a.actionExtensionManager, a.actionModules, a.actionHelp,
        a.actionOnlineDocs, a.actionShortcuts, a.actionAbout, a.actionReportProblem, a.actionExportDiagnostic,
    };
    for (const auto* list : { &a.drawTools, &a.advancedModifyTools, &a.modelDisplayModes, &a.renderModes, &a.dimensionCommands })
        all.insert(all.end(), list->begin(), list->end());
    for (const auto& [id, action] : a.dimensionTools) all.push_back(action);
    std::vector<QAction*> out;
    std::set<QAction*> seen;
    for (QAction* x : all)
        if (x && seen.insert(x).second) out.push_back(x);
    return out;
}

} // namespace TSA::UI
