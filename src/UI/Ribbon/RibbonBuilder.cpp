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
    buildStructureTab(bar, acts, parentWindow);
    buildAnalysisTab(bar, acts, parentWindow);
    buildViewTab(bar, acts, parentWindow);
}

RibbonTab* RibbonBuilder::buildHomeTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Accueil"));

    // 1. Groupe Fichier
    auto* filePanel = new RibbonPanel(QObject::tr("Fichier"), tab);
    if (acts.actionNew)
    {
        filePanel->addLargeAction(acts.actionNew);
    }
    if (acts.actionOpen && acts.actionSave)
    {
        filePanel->addInternalSeparator();
        filePanel->addSmallColumn({ acts.actionOpen, acts.actionSave });
    }
    tab->addPanel(filePanel);

    // 2. Groupe Sélection & Modification
    auto* selectPanel = new RibbonPanel(QObject::tr("Sélection & Édition"), tab);
    if (acts.actionSelectMode)
    {
        selectPanel->addLargeAction(acts.actionSelectMode);
    }
    std::vector<QAction*> modifyActions;
    if (acts.actionMove) modifyActions.push_back(acts.actionMove);
    if (acts.actionCopy) modifyActions.push_back(acts.actionCopy);
    if (acts.actionDelete) modifyActions.push_back(acts.actionDelete);
    if (!modifyActions.empty())
    {
        selectPanel->addInternalSeparator();
        selectPanel->addSmallColumn(modifyActions);
    }
    tab->addPanel(selectPanel);

    // 3. Groupe Géométrie & Nœuds
    auto* geomPanel = new RibbonPanel(QObject::tr("Nœuds & Primitives"), tab);
    if (acts.actionDrawNode)
    {
        geomPanel->addLargeAction(acts.actionDrawNode);
    }
    std::vector<QAction*> geomSub;
    if (acts.actionNewNode) geomSub.push_back(acts.actionNewNode);
    if (acts.actionAddCube) geomSub.push_back(acts.actionAddCube);
    if (!geomSub.empty())
    {
        geomPanel->addInternalSeparator();
        geomPanel->addSmallColumn(geomSub);
    }
    tab->addPanel(geomPanel);

    // 4. Groupe Éléments Structuraux
    auto* structPanel = new RibbonPanel(QObject::tr("Structure 3D"), tab);
    if (acts.actionDrawBeam)
    {
        structPanel->addLargeAction(acts.actionDrawBeam);
    }
    structPanel->addInternalSeparator();
    if (acts.actionDrawColumn)
    {
        structPanel->addLargeAction(acts.actionDrawColumn);
    }
    structPanel->addInternalSeparator();
    if (acts.actionDrawSlab)
    {
        structPanel->addLargeAction(acts.actionDrawSlab);
    }
    tab->addPanel(structPanel);

    // 5. Groupe Grilles & Niveaux
    auto* gridPanel = new RibbonPanel(QObject::tr("Grilles & Niveaux"), tab);
    if (acts.actionGridVisible)
    {
        gridPanel->addLargeAction(acts.actionGridVisible);
    }
    std::vector<QAction*> gridCol1;
    if (acts.actionGridSnap) gridCol1.push_back(acts.actionGridSnap);
    if (acts.actionGridManager) gridCol1.push_back(acts.actionGridManager);
    if (acts.actionManageLevels) gridCol1.push_back(acts.actionManageLevels);
    if (!gridCol1.empty())
    {
        gridPanel->addInternalSeparator();
        gridPanel->addSmallColumn(gridCol1);
    }
    tab->addPanel(gridPanel);

    // 6. Groupe Affichage Rapide
    auto* viewPanel = new RibbonPanel(QObject::tr("Orientation"), tab);
    if (acts.actionView3D)
    {
        viewPanel->addLargeAction(acts.actionView3D);
    }
    std::vector<QAction*> viewCol;
    if (acts.actionViewXY) viewCol.push_back(acts.actionViewXY);
    if (acts.actionFitAll) viewCol.push_back(acts.actionFitAll);
    if (acts.actionResetView) viewCol.push_back(acts.actionResetView);
    if (!viewCol.empty())
    {
        viewPanel->addInternalSeparator();
        viewPanel->addSmallColumn(viewCol);
    }
    tab->addPanel(viewPanel);

    return tab;
}

RibbonTab* RibbonBuilder::buildStructureTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Structure & Sections"));

    // 1. Éléments de structure
    auto* elemPanel = new RibbonPanel(QObject::tr("Éléments de Structure"), tab);
    if (acts.actionDrawBeam) elemPanel->addLargeAction(acts.actionDrawBeam);
    elemPanel->addInternalSeparator();
    if (acts.actionDrawColumn) elemPanel->addLargeAction(acts.actionDrawColumn);
    elemPanel->addInternalSeparator();
    if (acts.actionDrawSlab) elemPanel->addLargeAction(acts.actionDrawSlab);
    elemPanel->addInternalSeparator();

    auto* actWall = acts.actionWall ? acts.actionWall : new QAction(QIcon(":/icons/struct_wall.svg"), QObject::tr("Voile / Mur"), parentWindow);
    auto* actTruss = acts.actionTruss ? acts.actionTruss : new QAction(QIcon(":/icons/struct_truss.svg"), QObject::tr("Treillis"), parentWindow);
    auto* actFooting = acts.actionFooting ? acts.actionFooting : new QAction(QIcon(":/icons/struct_foundation.svg"), QObject::tr("Fondation"), parentWindow);

    elemPanel->addSmallColumn({ actWall, actTruss, actFooting });
    tab->addPanel(elemPanel);

    // 2. Sections Transversales
    auto* secPanel = new RibbonPanel(QObject::tr("Profilés & Sections"), tab);
    auto* actSecI = acts.actionSecI ? acts.actionSecI : new QAction(QIcon(":/icons/section_i.svg"), QObject::tr("Profilé I/H"), parentWindow);
    auto* actSecRect = acts.actionSecRect ? acts.actionSecRect : new QAction(QIcon(":/icons/section_rect.svg"), QObject::tr("Rectangulaire"), parentWindow);
    auto* actSecCirc = acts.actionSecCirc ? acts.actionSecCirc : new QAction(QIcon(":/icons/section_circle.svg"), QObject::tr("Circulaire"), parentWindow);

    secPanel->addLargeAction(actSecI);
    secPanel->addInternalSeparator();
    secPanel->addSmallColumn({ actSecRect, actSecCirc });
    tab->addPanel(secPanel);

    // 3. Matériaux
    auto* matPanel = new RibbonPanel(QObject::tr("Matériaux"), tab);
    auto* actConcrete = acts.actionConcrete ? acts.actionConcrete : new QAction(QIcon(":/icons/material_concrete.svg"), QObject::tr("Béton C25/30"), parentWindow);
    auto* actSteel = acts.actionSteel ? acts.actionSteel : new QAction(QIcon(":/icons/material_steel.svg"), QObject::tr("Acier S355"), parentWindow);

    matPanel->addLargeAction(actConcrete);
    matPanel->addInternalSeparator();
    matPanel->addSmallColumn({ actSteel });
    tab->addPanel(matPanel);

    // 4. Appuis & Conditions aux Limites
    auto* supPanel = new RibbonPanel(QObject::tr("Appuis & Liaisons"), tab);
    auto* actFixed = acts.actionFixed ? acts.actionFixed : new QAction(QIcon(":/icons/support_fixed.svg"), QObject::tr("Encastrement"), parentWindow);
    auto* actPinned = acts.actionPinned ? acts.actionPinned : new QAction(QIcon(":/icons/support_pinned.svg"), QObject::tr("Articulation"), parentWindow);
    auto* actRoller = acts.actionRoller ? acts.actionRoller : new QAction(QIcon(":/icons/support_roller.svg"), QObject::tr("Appui Simple"), parentWindow);

    supPanel->addLargeAction(actFixed);
    supPanel->addInternalSeparator();
    supPanel->addSmallColumn({ actPinned, actRoller });
    tab->addPanel(supPanel);

    return tab;
}

RibbonTab* RibbonBuilder::buildAnalysisTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Charges & Analyse"));

    // 1. Charges
    auto* loadPanel = new RibbonPanel(QObject::tr("Cas de Charges"), tab);
    auto* actPointLoad = acts.actionPointLoad ? acts.actionPointLoad : new QAction(QIcon(":/icons/load_point.svg"), QObject::tr("Force Ponctuelle"), parentWindow);
    auto* actDistLoad = acts.actionDistLoad ? acts.actionDistLoad : new QAction(QIcon(":/icons/load_dist.svg"), QObject::tr("Charge Répartie"), parentWindow);
    auto* actMoment = acts.actionMoment ? acts.actionMoment : new QAction(QIcon(":/icons/load_moment.svg"), QObject::tr("Moment"), parentWindow);
    auto* actSeismic = acts.actionSeismic ? acts.actionSeismic : new QAction(QIcon(":/icons/load_seismic.svg"), QObject::tr("Séisme / Vent"), parentWindow);

    loadPanel->addLargeAction(actPointLoad);
    loadPanel->addInternalSeparator();
    loadPanel->addSmallColumn({ actDistLoad, actMoment, actSeismic });
    tab->addPanel(loadPanel);

    // 2. Maillage
    auto* meshPanel = new RibbonPanel(QObject::tr("Maillage EF"), tab);
    auto* actGenMesh = acts.actionMeshGen ? acts.actionMeshGen : new QAction(QIcon(":/icons/mesh_generate.svg"), QObject::tr("Générer Maillage"), parentWindow);
    meshPanel->addLargeAction(actGenMesh);
    tab->addPanel(meshPanel);

    // 3. Calculs & Solveur
    auto* solvPanel = new RibbonPanel(QObject::tr("Solveur"), tab);
    auto* actRun = acts.actionRunSolve ? acts.actionRunSolve : new QAction(QIcon(":/icons/analysis_run.svg"), QObject::tr("Calculer"), parentWindow);
    auto* actModal = acts.actionModal ? acts.actionModal : new QAction(QIcon(":/icons/analysis_modal.svg"), QObject::tr("Analyse Modale"), parentWindow);

    solvPanel->addLargeAction(actRun);
    solvPanel->addInternalSeparator();
    solvPanel->addSmallColumn({ actModal });
    tab->addPanel(solvPanel);

    // 4. Résultats
    auto* resPanel = new RibbonPanel(QObject::tr("Résultats"), tab);
    auto* actDisp = acts.actionResultsDisp ? acts.actionResultsDisp : new QAction(QIcon(":/icons/results_disp.svg"), QObject::tr("Déplacements"), parentWindow);
    auto* actForces = acts.actionResultsForces ? acts.actionResultsForces : new QAction(QIcon(":/icons/results_force.svg"), QObject::tr("Diagrammes M/N/V"), parentWindow);
    auto* actStress = acts.actionResultsStress ? acts.actionResultsStress : new QAction(QIcon(":/icons/results_stress.svg"), QObject::tr("Contraintes"), parentWindow);

    resPanel->addLargeAction(actDisp);
    resPanel->addInternalSeparator();
    resPanel->addSmallColumn({ actForces, actStress });
    tab->addPanel(resPanel);

    return tab;
}

RibbonTab* RibbonBuilder::buildViewTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Vue & Outils"));

    // 1. Caméra & Projections
    auto* camPanel = new RibbonPanel(QObject::tr("Orientation & Zoom"), tab);
    if (acts.actionView3D) camPanel->addLargeAction(acts.actionView3D);
    std::vector<QAction*> colViews;
    if (acts.actionViewXY) colViews.push_back(acts.actionViewXY);
    if (acts.actionViewXZ) colViews.push_back(acts.actionViewXZ);
    if (acts.actionViewYZ) colViews.push_back(acts.actionViewYZ);
    if (!colViews.empty())
    {
        camPanel->addInternalSeparator();
        camPanel->addSmallColumn(colViews);
    }

    std::vector<QAction*> colZoom;
    if (acts.actionFitAll) colZoom.push_back(acts.actionFitAll);
    if (acts.actionResetView) colZoom.push_back(acts.actionResetView);
    if (!colZoom.empty())
    {
        camPanel->addInternalSeparator();
        camPanel->addSmallColumn(colZoom);
    }
    tab->addPanel(camPanel);

    // 2. Repères & Coupes 3D
    auto* coordPanel = new RibbonPanel(QObject::tr("Repères & Coupes"), tab);
    if (acts.actionCoordSystem) coordPanel->addLargeAction(acts.actionCoordSystem);
    coordPanel->addInternalSeparator();
    if (acts.actionSectionCut) coordPanel->addLargeAction(acts.actionSectionCut);
    tab->addPanel(coordPanel);

    // 3. Guides & Visibilité
    auto* visPanel = new RibbonPanel(QObject::tr("Guides & Règles"), tab);
    std::vector<QAction*> visCol1;
    if (acts.actionGridVisible) visCol1.push_back(acts.actionGridVisible);
    if (acts.actionLevelsVisible) visCol1.push_back(acts.actionLevelsVisible);
    if (acts.actionRulersVisible) visCol1.push_back(acts.actionRulersVisible);
    if (!visCol1.empty()) visPanel->addSmallColumn(visCol1);

    std::vector<QAction*> visCol2;
    if (acts.actionGridSnap) visCol2.push_back(acts.actionGridSnap);
    if (acts.actionGridLabels) visCol2.push_back(acts.actionGridLabels);
    if (!visCol2.empty())
    {
        visPanel->addInternalSeparator();
        visPanel->addSmallColumn(visCol2);
    }
    tab->addPanel(visPanel);

    // 4. Outils de Mesure
    auto* toolsPanel = new RibbonPanel(QObject::tr("Mesures"), tab);
    auto* actMeasure = acts.actionMeasure ? acts.actionMeasure : new QAction(QIcon(":/icons/measure.svg"), QObject::tr("Mesurer 3D"), parentWindow);
    toolsPanel->addLargeAction(actMeasure);
    tab->addPanel(toolsPanel);

    // 5. Panneaux Docks
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

    // 6. Style & Thème (Sombre / Clair)
    if (acts.actionToggleTheme)
    {
        auto* themePanel = new RibbonPanel(QObject::tr("Thème CAO"), tab);
        themePanel->addLargeAction(acts.actionToggleTheme);
        tab->addPanel(themePanel);
    }

    // 7. Aide & Documentation
    if (acts.actionHelp || acts.actionShortcuts || acts.actionAbout)
    {
        auto* helpPanel = new RibbonPanel(QObject::tr("Aide & Support"), tab);
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
