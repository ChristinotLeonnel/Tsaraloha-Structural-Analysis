#include "RibbonBuilder.h"
#include "RibbonBar.h"
#include "RibbonTab.h"
#include "RibbonPanel.h"
#include "RibbonButton.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>

namespace TSA::UI
{

void RibbonBuilder::buildAllTabs(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    if (!bar) return;

    buildHomeTab(bar, acts, parentWindow);
    buildModelingTab(bar, acts, parentWindow);
    buildStructureTab(bar, acts, parentWindow);
    buildCalculationTab(bar, acts, parentWindow);
    buildResultsTab(bar, acts, parentWindow);
    buildEditTab(bar, acts, parentWindow);
    buildViewTab(bar, acts, parentWindow);
    buildToolsTab(bar, acts, parentWindow);
}

// -----------------------------------------------------------------------------
// 1. Onglet ACCUEIL
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildHomeTab(RibbonBar* bar, const RibbonActions& acts, QWidget* /*parentWindow*/)
{
    auto* tab = bar->addTab(QObject::tr("Accueil"));

    // Groupe Projet / Fichier
    auto* filePanel = new RibbonPanel(QObject::tr("Projet"), tab);
    if (acts.actionNew) filePanel->addLargeAction(acts.actionNew);
    std::vector<QAction*> fileSub;
    if (acts.actionOpen) fileSub.push_back(acts.actionOpen);
    if (acts.actionSave) fileSub.push_back(acts.actionSave);
    if (acts.actionSaveAs) fileSub.push_back(acts.actionSaveAs);
    if (!fileSub.empty())
    {
        filePanel->addInternalSeparator();
        filePanel->addSmallColumn(fileSub);
    }
    tab->addPanel(filePanel);

    // Groupe Historique & Presse-papier
    auto* clipPanel = new RibbonPanel(QObject::tr("Historique"), tab);
    std::vector<QAction*> histCol;
    if (acts.actionUndo) histCol.push_back(acts.actionUndo);
    if (acts.actionRedo) histCol.push_back(acts.actionRedo);
    if (!histCol.empty()) clipPanel->addSmallColumn(histCol);

    std::vector<QAction*> clipCol;
    if (acts.actionCopyClipboard) clipCol.push_back(acts.actionCopyClipboard);
    if (acts.actionPasteClipboard) clipCol.push_back(acts.actionPasteClipboard);
    if (!clipCol.empty())
    {
        clipPanel->addInternalSeparator();
        clipPanel->addSmallColumn(clipCol);
    }
    tab->addPanel(clipPanel);

    // Groupe Accès Rapide
    auto* quickPanel = new RibbonPanel(QObject::tr("Accès Rapide"), tab);
    if (acts.actionSelectMode) quickPanel->addLargeAction(acts.actionSelectMode);
    if (acts.actionDrawBeam)
    {
        quickPanel->addInternalSeparator();
        quickPanel->addLargeAction(acts.actionDrawBeam);
    }
    if (acts.actionDrawColumn) quickPanel->addLargeAction(acts.actionDrawColumn);
    if (acts.actionDrawSlab) quickPanel->addLargeAction(acts.actionDrawSlab);
    if (acts.actionRunSolve)
    {
        quickPanel->addInternalSeparator();
        quickPanel->addLargeAction(acts.actionRunSolve);
    }
    tab->addPanel(quickPanel);

    // Groupe Vue Rapide
    auto* viewPanel = new RibbonPanel(QObject::tr("Vue 3D"), tab);
    if (acts.actionView3D) viewPanel->addLargeAction(acts.actionView3D);
    std::vector<QAction*> vCol;
    if (acts.actionFitAll) vCol.push_back(acts.actionFitAll);
    if (acts.actionResetView) vCol.push_back(acts.actionResetView);
    if (!vCol.empty())
    {
        viewPanel->addInternalSeparator();
        viewPanel->addSmallColumn(vCol);
    }
    tab->addPanel(viewPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 2. Onglet MODÉLISATION
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildModelingTab(RibbonBar* bar, const RibbonActions& acts, QWidget* /*parentWindow*/)
{
    auto* tab = bar->addTab(QObject::tr("Modélisation"));

    // Éléments Filaires (1D)
    auto* beamPanel = new RibbonPanel(QObject::tr("Éléments Filaires (1D)"), tab);
    if (acts.actionDrawBeam) beamPanel->addLargeAction(acts.actionDrawBeam);
    if (acts.actionDrawColumn) beamPanel->addLargeAction(acts.actionDrawColumn);

    std::vector<QAction*> wireSub;
    if (acts.actionDrawBar) wireSub.push_back(acts.actionDrawBar);
    if (acts.actionTruss) wireSub.push_back(acts.actionTruss);
    if (!wireSub.empty())
    {
        beamPanel->addInternalSeparator();
        beamPanel->addSmallColumn(wireSub);
    }
    tab->addPanel(beamPanel);

    // Éléments Surfaciques (2D)
    auto* surfPanel = new RibbonPanel(QObject::tr("Éléments Surfaciques (2D)"), tab);
    if (acts.actionDrawSlab) surfPanel->addLargeAction(acts.actionDrawSlab);
    if (acts.actionDrawWall) surfPanel->addLargeAction(acts.actionDrawWall);
    if (acts.actionFooting)
    {
        surfPanel->addInternalSeparator();
        surfPanel->addSmallColumn({ acts.actionFooting });
    }
    tab->addPanel(surfPanel);

    // Nœuds & Primitives
    auto* nodePanel = new RibbonPanel(QObject::tr("Nœuds & Primitives"), tab);
    if (acts.actionDrawNode) nodePanel->addLargeAction(acts.actionDrawNode);
    std::vector<QAction*> nodeSub;
    if (acts.actionNewNode) nodeSub.push_back(acts.actionNewNode);
    if (acts.actionAddCube) nodeSub.push_back(acts.actionAddCube);
    if (!nodeSub.empty())
    {
        nodePanel->addInternalSeparator();
        nodePanel->addSmallColumn(nodeSub);
    }
    tab->addPanel(nodePanel);

    // Grilles & Niveaux
    auto* gridPanel = new RibbonPanel(QObject::tr("Trame & Niveaux"), tab);
    if (acts.actionNewGrid) gridPanel->addLargeAction(acts.actionNewGrid);
    std::vector<QAction*> gSub;
    if (acts.actionGridManager) gSub.push_back(acts.actionGridManager);
    if (acts.actionManageLevels) gSub.push_back(acts.actionManageLevels);
    if (!gSub.empty())
    {
        gridPanel->addInternalSeparator();
        gridPanel->addSmallColumn(gSub);
    }
    tab->addPanel(gridPanel);

    // Paramètres
    if (acts.actionStructurePresets)
    {
        auto* cfgPanel = new RibbonPanel(QObject::tr("Préréglages"), tab);
        cfgPanel->addLargeAction(acts.actionStructurePresets);
        tab->addPanel(cfgPanel);
    }

    return tab;
}

// -----------------------------------------------------------------------------
// 3. Onglet STRUCTURE
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildStructureTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Structure"));

    // Sections & Profilés
    auto* secPanel = new RibbonPanel(QObject::tr("Sections & Profilés"), tab);
    auto* actSecI = acts.actionSecI ? acts.actionSecI : new QAction(QIcon(":/icons/section_i.svg"), QObject::tr("Profilé I/H"), parentWindow);
    auto* actSecRect = acts.actionSecRect ? acts.actionSecRect : new QAction(QIcon(":/icons/section_rect.svg"), QObject::tr("Rectangulaire"), parentWindow);
    auto* actSecCirc = acts.actionSecCirc ? acts.actionSecCirc : new QAction(QIcon(":/icons/section_circle.svg"), QObject::tr("Circulaire"), parentWindow);

    secPanel->addLargeAction(actSecI);
    secPanel->addInternalSeparator();
    secPanel->addSmallColumn({ actSecRect, actSecCirc });
    tab->addPanel(secPanel);

    // Matériaux
    auto* matPanel = new RibbonPanel(QObject::tr("Matériaux"), tab);
    auto* actConcrete = acts.actionConcrete ? acts.actionConcrete : new QAction(QIcon(":/icons/material_concrete.svg"), QObject::tr("Béton Armé (EC2)"), parentWindow);
    auto* actSteel = acts.actionSteel ? acts.actionSteel : new QAction(QIcon(":/icons/material_steel.svg"), QObject::tr("Acier Structural (EC3)"), parentWindow);

    matPanel->addLargeAction(actConcrete);
    matPanel->addInternalSeparator();
    matPanel->addSmallColumn({ actSteel });
    tab->addPanel(matPanel);

    // Conditions d'Appuis
    auto* supPanel = new RibbonPanel(QObject::tr("Conditions d'Appuis"), tab);
    auto* actFixed = acts.actionFixed ? acts.actionFixed : new QAction(QIcon(":/icons/support_fixed.svg"), QObject::tr("Encastrement"), parentWindow);
    auto* actPinned = acts.actionPinned ? acts.actionPinned : new QAction(QIcon(":/icons/support_pinned.svg"), QObject::tr("Articulation"), parentWindow);
    auto* actRoller = acts.actionRoller ? acts.actionRoller : new QAction(QIcon(":/icons/support_roller.svg"), QObject::tr("Appui Simple"), parentWindow);

    supPanel->addLargeAction(actFixed);
    supPanel->addInternalSeparator();
    supPanel->addSmallColumn({ actPinned, actRoller });
    tab->addPanel(supPanel);

    // Bibliothèque Personnalisée
    auto* libPanel = new RibbonPanel(QObject::tr("Bibliothèque"), tab);
    auto* actLib = acts.actionLibrary ? acts.actionLibrary : new QAction(QIcon(":/icons/structure_preset.svg"), QObject::tr("Bibliothèque..."), parentWindow);
    libPanel->addLargeAction(actLib);
    tab->addPanel(libPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 4. Onglet CALCUL
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildCalculationTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Calcul"));

    // Actions & Charges
    auto* loadPanel = new RibbonPanel(QObject::tr("Actions & Charges"), tab);
    auto* actPointLoad = acts.actionPointLoad ? acts.actionPointLoad : new QAction(QIcon(":/icons/load_point.svg"), QObject::tr("Force Ponctuelle"), parentWindow);
    auto* actDistLoad = acts.actionDistLoad ? acts.actionDistLoad : new QAction(QIcon(":/icons/load_dist.svg"), QObject::tr("Charge Répartie"), parentWindow);
    auto* actMoment = acts.actionMoment ? acts.actionMoment : new QAction(QIcon(":/icons/load_moment.svg"), QObject::tr("Moment"), parentWindow);
    auto* actSeismic = acts.actionSeismic ? acts.actionSeismic : new QAction(QIcon(":/icons/load_seismic.svg"), QObject::tr("Séisme (EC8)"), parentWindow);

    loadPanel->addLargeAction(actPointLoad);
    loadPanel->addInternalSeparator();
    loadPanel->addSmallColumn({ actDistLoad, actMoment, actSeismic });
    tab->addPanel(loadPanel);

    // Maillage Éléments Finis
    auto* meshPanel = new RibbonPanel(QObject::tr("Discrétisation"), tab);
    auto* actGenMesh = acts.actionMeshGen ? acts.actionMeshGen : new QAction(QIcon(":/icons/mesh_generate.svg"), QObject::tr("Générer Maillage"), parentWindow);
    meshPanel->addLargeAction(actGenMesh);
    tab->addPanel(meshPanel);

    // Solveur
    auto* solvPanel = new RibbonPanel(QObject::tr("Solveur"), tab);
    auto* actRun = acts.actionRunSolve ? acts.actionRunSolve : new QAction(QIcon(":/icons/analysis_run.svg"), QObject::tr("Calcul Statique"), parentWindow);
    auto* actModal = acts.actionModal ? acts.actionModal : new QAction(QIcon(":/icons/analysis_modal.svg"), QObject::tr("Analyse Modale"), parentWindow);

    solvPanel->addLargeAction(actRun);
    solvPanel->addInternalSeparator();
    solvPanel->addSmallColumn({ actModal });
    tab->addPanel(solvPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 5. Onglet RÉSULTATS
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildResultsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Résultats"));

    // Déformations
    auto* defPanel = new RibbonPanel(QObject::tr("Déformations"), tab);
    auto* actDisp = acts.actionResultsDisp ? acts.actionResultsDisp : new QAction(QIcon(":/icons/results_disp.svg"), QObject::tr("Déplacements"), parentWindow);
    defPanel->addLargeAction(actDisp);
    tab->addPanel(defPanel);

    // Efforts Internes
    auto* forcePanel = new RibbonPanel(QObject::tr("Efforts Internes"), tab);
    auto* actForces = acts.actionResultsForces ? acts.actionResultsForces : new QAction(QIcon(":/icons/results_force.svg"), QObject::tr("Diagrammes M/N/V"), parentWindow);
    forcePanel->addLargeAction(actForces);
    tab->addPanel(forcePanel);

    // Contraintes
    auto* stressPanel = new RibbonPanel(QObject::tr("Contraintes"), tab);
    auto* actStress = acts.actionResultsStress ? acts.actionResultsStress : new QAction(QIcon(":/icons/results_stress.svg"), QObject::tr("Von Mises (σ_vm)"), parentWindow);
    stressPanel->addLargeAction(actStress);
    tab->addPanel(stressPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 6. Onglet ÉDITION
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildEditTab(RibbonBar* bar, const RibbonActions& acts, QWidget* /*parentWindow*/)
{
    auto* tab = bar->addTab(QObject::tr("Édition"));

    // Sélection
    auto* selPanel = new RibbonPanel(QObject::tr("Sélection"), tab);
    if (acts.actionSelectMode) selPanel->addLargeAction(acts.actionSelectMode);
    tab->addPanel(selPanel);

    // Déplacement
    auto* movePanel = new RibbonPanel(QObject::tr("Déplacement"), tab);
    if (acts.actionMove3D) movePanel->addLargeAction(acts.actionMove3D);
    if (acts.actionMove)
    {
        movePanel->addInternalSeparator();
        movePanel->addSmallColumn({ acts.actionMove });
    }
    tab->addPanel(movePanel);

    // Copie & Répétition
    auto* copyPanel = new RibbonPanel(QObject::tr("Copie & Duplication"), tab);
    if (acts.actionCopy3D) copyPanel->addLargeAction(acts.actionCopy3D);
    std::vector<QAction*> copySub;
    if (acts.actionCopy) copySub.push_back(acts.actionCopy);
    if (acts.actionRotate3D) copySub.push_back(acts.actionRotate3D);
    if (!copySub.empty())
    {
        copyPanel->addInternalSeparator();
        copyPanel->addSmallColumn(copySub);
    }
    tab->addPanel(copyPanel);

    // Repère de Travail
    if (acts.actionMoveOrigin)
    {
        auto* origPanel = new RibbonPanel(QObject::tr("Repère"), tab);
        origPanel->addLargeAction(acts.actionMoveOrigin);
        tab->addPanel(origPanel);
    }

    // Presse-papier
    std::vector<QAction*> clipCol;
    if (acts.actionCopyClipboard) clipCol.push_back(acts.actionCopyClipboard);
    if (acts.actionPasteClipboard) clipCol.push_back(acts.actionPasteClipboard);
    if (!clipCol.empty())
    {
        auto* clipPanel = new RibbonPanel(QObject::tr("Presse-papier"), tab);
        clipPanel->addSmallColumn(clipCol);
        tab->addPanel(clipPanel);
    }

    // Suppression
    if (acts.actionDelete)
    {
        auto* delPanel = new RibbonPanel(QObject::tr("Suppression"), tab);
        delPanel->addLargeAction(acts.actionDelete);
        tab->addPanel(delPanel);
    }

    return tab;
}

// -----------------------------------------------------------------------------
// 7. Onglet AFFICHAGE
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildViewTab(RibbonBar* bar, const RibbonActions& acts, QWidget* /*parentWindow*/)
{
    auto* tab = bar->addTab(QObject::tr("Affichage"));

    // Projections
    auto* projPanel = new RibbonPanel(QObject::tr("Projections"), tab);
    if (acts.actionView3D) projPanel->addLargeAction(acts.actionView3D);
    std::vector<QAction*> colViews;
    if (acts.actionViewXY) colViews.push_back(acts.actionViewXY);
    if (acts.actionViewXZ) colViews.push_back(acts.actionViewXZ);
    if (acts.actionViewYZ) colViews.push_back(acts.actionViewYZ);
    if (!colViews.empty())
    {
        projPanel->addInternalSeparator();
        projPanel->addSmallColumn(colViews);
    }
    tab->addPanel(projPanel);

    // Navigation & Cadrage
    std::vector<QAction*> colZoom;
    if (acts.actionFitAll) colZoom.push_back(acts.actionFitAll);
    if (acts.actionResetView) colZoom.push_back(acts.actionResetView);
    if (!colZoom.empty())
    {
        auto* navPanel = new RibbonPanel(QObject::tr("Navigation"), tab);
        navPanel->addSmallColumn(colZoom);
        tab->addPanel(navPanel);
    }

    // Repères & Coupes
    auto* cutPanel = new RibbonPanel(QObject::tr("Plans & Coupes"), tab);
    if (acts.actionCoordSystem) cutPanel->addLargeAction(acts.actionCoordSystem);
    if (acts.actionSectionCut)
    {
        cutPanel->addInternalSeparator();
        cutPanel->addLargeAction(acts.actionSectionCut);
    }
    tab->addPanel(cutPanel);

    // Aides Visuelles
    auto* visPanel = new RibbonPanel(QObject::tr("Aides Visuelles"), tab);
    std::vector<QAction*> visCol1;
    if (acts.actionGridVisible) visCol1.push_back(acts.actionGridVisible);
    if (acts.actionLevelsVisible) visCol1.push_back(acts.actionLevelsVisible);
    if (acts.actionRulersVisible) visCol1.push_back(acts.actionRulersVisible);
    if (!visCol1.empty()) visPanel->addSmallColumn(visCol1);

    std::vector<QAction*> visCol2;
    if (acts.actionGridSnap) visCol2.push_back(acts.actionGridSnap);
    if (acts.actionGridLabels) visCol2.push_back(acts.actionGridLabels);
    if (acts.actionFullScreen) visCol2.push_back(acts.actionFullScreen);
    if (!visCol2.empty())
    {
        visPanel->addInternalSeparator();
        visPanel->addSmallColumn(visCol2);
    }
    tab->addPanel(visPanel);

    // Fenêtres Docks
    auto* dockPanel = new RibbonPanel(QObject::tr("Fenêtres & Docks"), tab);
    std::vector<QAction*> dockCol1;
    if (acts.actionToggleModelTree) dockCol1.push_back(acts.actionToggleModelTree);
    if (acts.actionToggleProperties) dockCol1.push_back(acts.actionToggleProperties);
    if (!dockCol1.empty()) dockPanel->addSmallColumn(dockCol1);

    std::vector<QAction*> dockCol2;
    if (acts.actionToggleVisibility) dockCol2.push_back(acts.actionToggleVisibility);
    if (acts.actionToggleConsole) dockCol2.push_back(acts.actionToggleConsole);
    if (!dockCol2.empty())
    {
        dockPanel->addInternalSeparator();
        dockPanel->addSmallColumn(dockCol2);
    }
    tab->addPanel(dockPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 8. Onglet OUTILS
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildToolsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Outils"));

    // Mesures
    auto* actMeasure = acts.actionMeasure ? acts.actionMeasure : new QAction(QIcon(":/icons/measure.svg"), QObject::tr("Mesurer 3D"), parentWindow);
    auto* measPanel = new RibbonPanel(QObject::tr("Inspection"), tab);
    measPanel->addLargeAction(actMeasure);
    tab->addPanel(measPanel);

    // Préférences & Thème
    if (acts.actionToggleTheme)
    {
        auto* envPanel = new RibbonPanel(QObject::tr("Environnement"), tab);
        envPanel->addLargeAction(acts.actionToggleTheme);
        tab->addPanel(envPanel);
    }

    // Documentation & Aide
    if (acts.actionHelp || acts.actionShortcuts || acts.actionAbout)
    {
        auto* helpPanel = new RibbonPanel(QObject::tr("Documentation"), tab);
        if (acts.actionHelp) helpPanel->addLargeAction(acts.actionHelp);

        std::vector<QAction*> helpCol;
        if (acts.actionShortcuts) helpCol.push_back(acts.actionShortcuts);
        if (acts.actionAbout) helpCol.push_back(acts.actionAbout);
        if (!helpCol.empty())
        {
            helpPanel->addInternalSeparator();
            helpPanel->addSmallColumn(helpCol);
        }
        tab->addPanel(helpPanel);
    }

    return tab;
}

} // namespace TSA::UI
