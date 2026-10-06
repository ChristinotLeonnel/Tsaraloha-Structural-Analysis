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

    // Barre d'accès rapide : les commandes de tous les jours, quel que soit l'onglet actif.
    // Nouveau, Ouvrir, Enregistrer, Annuler et Rétablir sont dans la barre de titre (AppShell).
    bar->setQuickAccess({ acts.actionSelectMode, acts.actionFitAll, acts.actionView3D,
                          acts.actionRunSolve });

    // Ordre : fichier → modèle → modification → structure → charges → analyse → résultats → vues → outils.
    buildHomeTab(bar, acts, parentWindow);
    buildModelingTab(bar, acts, parentWindow);
    buildEditTab(bar, acts, parentWindow);
    buildStructureTab(bar, acts, parentWindow);
    buildLoadsTab(bar, acts, parentWindow);
    buildAnalysisTab(bar, acts, parentWindow);
    buildResultsTab(bar, acts, parentWindow);
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
    if (acts.actionImportIfc || acts.actionExportIfc)
    {
        std::vector<QAction*> ifcSub;
        if (acts.actionImportIfc) ifcSub.push_back(acts.actionImportIfc);
        if (acts.actionExportIfc) ifcSub.push_back(acts.actionExportIfc);
        filePanel->addInternalSeparator();
        filePanel->addSmallColumn(ifcSub);
    }
    if (acts.actionCloseProject)
    {
        filePanel->addInternalSeparator();
        filePanel->addLargeAction(acts.actionCloseProject);
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

    // Édition courante
    auto* editPanel = new RibbonPanel(QObject::tr("Édition"), tab);
    if (acts.actionSelectMode) editPanel->addLargeAction(acts.actionSelectMode);
    std::vector<QAction*> editCol;
    if (acts.actionMove3D) editCol.push_back(acts.actionMove3D);
    if (acts.actionCopy3D) editCol.push_back(acts.actionCopy3D);
    if (acts.actionDelete) editCol.push_back(acts.actionDelete);
    if (!editCol.empty())
    {
        editPanel->addInternalSeparator();
        editPanel->addSmallColumn(editCol);
    }
    tab->addPanel(editPanel);

    // Modélisation : éléments structuraux principaux
    auto* modelPanel = new RibbonPanel(QObject::tr("Structure"), tab);
    if (acts.actionDrawBeam) modelPanel->addLargeAction(acts.actionDrawBeam);
    if (acts.actionDrawColumn) modelPanel->addLargeAction(acts.actionDrawColumn);
    if (acts.actionDrawSlab) modelPanel->addLargeAction(acts.actionDrawSlab);
    if (acts.actionDrawWall) modelPanel->addLargeAction(acts.actionDrawWall);
    std::vector<QAction*> modelCol;
    if (acts.actionDrawNode) modelCol.push_back(acts.actionDrawNode);
    if (acts.actionFooting) modelCol.push_back(acts.actionFooting);
    if (acts.actionDrawCable) modelCol.push_back(acts.actionDrawCable);
    if (!modelCol.empty())
    {
        modelPanel->addInternalSeparator();
        modelPanel->addSmallColumn(modelCol);
    }
    tab->addPanel(modelPanel);

    // Appuis & charges
    auto* bcPanel = new RibbonPanel(QObject::tr("Appuis & Charges"), tab);
    if (acts.actionFixed) bcPanel->addLargeAction(acts.actionFixed);
    if (acts.actionPointLoad) bcPanel->addLargeAction(acts.actionPointLoad);
    if (acts.actionDistLoad) bcPanel->addLargeAction(acts.actionDistLoad);
    std::vector<QAction*> bcCol;
    if (acts.actionPinned) bcCol.push_back(acts.actionPinned);
    if (acts.actionRoller) bcCol.push_back(acts.actionRoller);
    if (acts.actionLoadCases) bcCol.push_back(acts.actionLoadCases);
    if (!bcCol.empty())
    {
        bcPanel->addInternalSeparator();
        bcPanel->addSmallColumn(bcCol);
    }
    tab->addPanel(bcPanel);

    // Analyse & résultats
    auto* calcPanel = new RibbonPanel(QObject::tr("Analyse"), tab);
    if (acts.actionMeshGen) calcPanel->addLargeAction(acts.actionMeshGen);
    if (acts.actionRunSolve) calcPanel->addLargeAction(acts.actionRunSolve);
    if (acts.actionResultsDock) calcPanel->addLargeAction(acts.actionResultsDock);
    std::vector<QAction*> calcCol;
    if (acts.actionAnalysisConfig) calcCol.push_back(acts.actionAnalysisConfig);
    if (acts.actionDeformedToggle) calcCol.push_back(acts.actionDeformedToggle);
    if (acts.actionOpenNDC) calcCol.push_back(acts.actionOpenNDC);
    if (!calcCol.empty())
    {
        calcPanel->addInternalSeparator();
        calcPanel->addSmallColumn(calcCol);
    }
    tab->addPanel(calcPanel);

    // Vue
    auto* viewPanel = new RibbonPanel(QObject::tr("Vue"), tab);
    if (acts.actionView3D) viewPanel->addLargeAction(acts.actionView3D);
    std::vector<QAction*> vCol;
    if (acts.actionFitAll) vCol.push_back(acts.actionFitAll);
    if (acts.actionFitSelection) vCol.push_back(acts.actionFitSelection);
    if (acts.actionResetView) vCol.push_back(acts.actionResetView);
    if (!vCol.empty())
    {
        viewPanel->addInternalSeparator();
        viewPanel->addSmallColumn(vCol);
    }
    std::vector<QAction*> panelCol;
    if (acts.actionToggleModelTree) panelCol.push_back(acts.actionToggleModelTree);
    if (acts.actionToggleProperties) panelCol.push_back(acts.actionToggleProperties);
    if (acts.actionToggleVisibility) panelCol.push_back(acts.actionToggleVisibility);
    if (!panelCol.empty())
    {
        viewPanel->addInternalSeparator();
        viewPanel->addSmallColumn(panelCol);
    }
    tab->addPanel(viewPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 2. Onglet MODÉLISATION
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildModelingTab(RibbonBar* bar, const RibbonActions& acts, QWidget* /*parentWindow*/)
{
    auto* tab = bar->addTab(QObject::tr("Modèle"));

    // Éléments Filaires (1D)
    auto* beamPanel = new RibbonPanel(QObject::tr("Éléments Filaires (1D)"), tab);
    if (acts.actionDrawBeam) beamPanel->addLargeAction(acts.actionDrawBeam);
    if (acts.actionDrawColumn) beamPanel->addLargeAction(acts.actionDrawColumn);
    if (acts.actionDrawCable) beamPanel->addLargeAction(acts.actionDrawCable);

    std::vector<QAction*> wireSub;
    if (acts.actionDrawBar) wireSub.push_back(acts.actionDrawBar);
    if (acts.actionTruss) wireSub.push_back(acts.actionTruss);
    if (!wireSub.empty())
    {
        beamPanel->addInternalSeparator();
        beamPanel->addSmallColumn(wireSub);
    }
    tab->addPanel(beamPanel);

    // Dessin rapide (registre des outils de dessin)
    if (!acts.drawTools.empty())
    {
        auto* drawPanel = new RibbonPanel(QObject::tr("Dessin rapide"), tab);
        for (std::size_t i = 0; i < acts.drawTools.size(); i += 3)
        {
            if (i) drawPanel->addInternalSeparator();
            std::vector<QAction*> col(acts.drawTools.begin() + i, acts.drawTools.begin() + std::min(i + 3, acts.drawTools.size()));
            drawPanel->addSmallColumn(col);
        }
        tab->addPanel(drawPanel);
    }

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

    // Appuis (aussi accessibles depuis l'onglet Structure)
    if (acts.actionFixed || acts.actionPinned || acts.actionRoller)
    {
        auto* supPanel = new RibbonPanel(QObject::tr("Appuis"), tab);
        if (acts.actionFixed) supPanel->addLargeAction(acts.actionFixed);
        std::vector<QAction*> supCol;
        if (acts.actionPinned) supCol.push_back(acts.actionPinned);
        if (acts.actionRoller) supCol.push_back(acts.actionRoller);
        if (!supCol.empty())
        {
            supPanel->addInternalSeparator();
            supPanel->addSmallColumn(supCol);
        }
        tab->addPanel(supPanel);
    }

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

    // Bibliothèques & Extensions TSALib
    auto* libPanel = new RibbonPanel(QObject::tr("Bibliothèques & TSALib"), tab);
    auto* actExtMgr = acts.actionExtensionManager ? acts.actionExtensionManager : new QAction(QIcon(":/icons/file_new.svg"), QObject::tr("Gestionnaire TSALib..."), parentWindow);
    auto* actLib = acts.actionLibrary ? acts.actionLibrary : new QAction(QIcon(":/icons/structure_preset.svg"), QObject::tr("Bibliothèque..."), parentWindow);
    libPanel->addLargeAction(actExtMgr);
    libPanel->addLargeAction(actLib);
    tab->addPanel(libPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 4. Onglet CHARGES
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildLoadsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Charges"));

    // Groupe 1 : Actions Ponctuelles
    auto* ptPanel = new RibbonPanel(QObject::tr("Actions Ponctuelles"), tab);
    auto* actPointLoad = acts.actionPointLoad ? acts.actionPointLoad : new QAction(QIcon(":/icons/load_point.svg"), QObject::tr("Force Ponctuelle"), parentWindow);
    auto* actMoment = acts.actionMoment ? acts.actionMoment : new QAction(QIcon(":/icons/load_moment.svg"), QObject::tr("Moment"), parentWindow);
    ptPanel->addLargeAction(actPointLoad);
    ptPanel->addLargeAction(actMoment);
    tab->addPanel(ptPanel);

    // Groupe 2 : Actions Réparties
    auto* distPanel = new RibbonPanel(QObject::tr("Actions Réparties"), tab);
    auto* actDistLoad = acts.actionDistLoad ? acts.actionDistLoad : new QAction(QIcon(":/icons/load_dist.svg"), QObject::tr("Charge Répartie"), parentWindow);
    distPanel->addLargeAction(actDistLoad);
    tab->addPanel(distPanel);

    // Groupe 3 : Cas de Charges & Normes
    auto* casesPanel = new RibbonPanel(QObject::tr("Cas & Normes"), tab);
    if (acts.actionLoadCases)
    {
        casesPanel->addLargeAction(acts.actionLoadCases);
    }
    auto* actSeismic = acts.actionSeismic ? acts.actionSeismic : new QAction(QIcon(":/icons/load_seismic.svg"), QObject::tr("Séisme (EC8)"), parentWindow);
    casesPanel->addLargeAction(actSeismic);
    tab->addPanel(casesPanel);

    // Groupe 4 : Affichage 3D des Charges
    auto* visPanel = new RibbonPanel(QObject::tr("Affichage 3D"), tab);
    if (acts.actionLoadsVisible)
    {
        visPanel->addLargeAction(acts.actionLoadsVisible);
    }
    std::vector<QAction*> visSub;
    if (acts.actionForcesVisible) visSub.push_back(acts.actionForcesVisible);
    if (acts.actionMomentsVisible) visSub.push_back(acts.actionMomentsVisible);
    if (acts.actionLoadValuesVisible) visSub.push_back(acts.actionLoadValuesVisible);
    if (!visSub.empty())
    {
        visPanel->addInternalSeparator();
        visPanel->addSmallColumn(visSub);
    }
    tab->addPanel(visPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 5. Onglet ANALYSE
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildAnalysisTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Analyse"));

    // Groupe 1 : Discrétisation EF
    auto* meshPanel = new RibbonPanel(QObject::tr("Discrétisation"), tab);
    auto* actGenMesh = acts.actionMeshGen ? acts.actionMeshGen : new QAction(QIcon(":/icons/mesh_generate.svg"), QObject::tr("Générer Maillage"), parentWindow);
    meshPanel->addLargeAction(actGenMesh);
    tab->addPanel(meshPanel);

    // Groupe 2 : Résolution & Solveur EF
    auto* solvPanel = new RibbonPanel(QObject::tr("Solveur EF"), tab);
    auto* actRun = acts.actionRunSolve ? acts.actionRunSolve : new QAction(QIcon(":/icons/analysis_run.svg"), QObject::tr("Calcul Statique"), parentWindow);
    auto* actModal = acts.actionModal ? acts.actionModal : new QAction(QIcon(":/icons/analysis_modal.svg"), QObject::tr("Analyse Modale"), parentWindow);
    auto* actPush = acts.actionPushover ? acts.actionPushover : new QAction(QIcon(":/icons/analysis_pushover.svg"), QObject::tr("Pushover"), parentWindow);

    solvPanel->addLargeAction(actRun);
    solvPanel->addInternalSeparator();
    solvPanel->addSmallColumn({ actModal, actPush });
    tab->addPanel(solvPanel);

    // Groupe 3 : Configuration & Paramètres
    auto* paramPanel = new RibbonPanel(QObject::tr("Paramètres"), tab);
    if (acts.actionAnalysisConfig)
    {
        paramPanel->addLargeAction(acts.actionAnalysisConfig);
    }
    tab->addPanel(paramPanel);

    // Groupe IA : vérification et analyse du modèle par l'assistant
    if (acts.actionAICheck || acts.actionAIAnalyze)
    {
        auto* aiPanel = new RibbonPanel(QObject::tr("Co-Engineering"), tab);
        if (acts.actionAICheck) aiPanel->addLargeAction(acts.actionAICheck);
        std::vector<QAction*> aiCol;
        if (acts.actionAIAnalyze) aiCol.push_back(acts.actionAIAnalyze);
        if (acts.actionAIExplain) aiCol.push_back(acts.actionAIExplain);
        if (acts.actionAIAssistant) aiCol.push_back(acts.actionAIAssistant);
        if (!aiCol.empty())
        {
            aiPanel->addInternalSeparator();
            aiPanel->addSmallColumn(aiCol);
        }
        tab->addPanel(aiPanel);
    }

    // Groupe 4 : Résultats Rapides (Panneau)
    if (acts.actionResultsDock)
    {
        auto* resPanel = new RibbonPanel(QObject::tr("Panneau Résultats"), tab);
        resPanel->addLargeAction(acts.actionResultsDock);
        tab->addPanel(resPanel);
    }

    return tab;
}

// -----------------------------------------------------------------------------
// Ancien onglet CALCUL (conservé pour rétrocompatibilité)
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildCalculationTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    buildLoadsTab(bar, acts, parentWindow);
    return buildAnalysisTab(bar, acts, parentWindow);
}

// -----------------------------------------------------------------------------
// 5. Onglet RÉSULTATS
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildResultsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow)
{
    auto* tab = bar->addTab(QObject::tr("Résultats"));

    // Panneau de contrôle des Résultats 3D (Dock)
    if (acts.actionResultsDock)
    {
        auto* dockPanel = new RibbonPanel(QObject::tr("Panneau"), tab);
        dockPanel->addLargeAction(acts.actionResultsDock);
        tab->addPanel(dockPanel);
    }

    // Déformations & Déplacements
    auto* defPanel = new RibbonPanel(QObject::tr("Déformée 3D"), tab);
    if (acts.actionDeformedToggle)
    {
        defPanel->addLargeAction(acts.actionDeformedToggle);
    }
    else
    {
        auto* actDisp = acts.actionResultsDisp ? acts.actionResultsDisp : new QAction(QIcon(":/icons/results_disp.svg"), QObject::tr("Déplacements"), parentWindow);
        defPanel->addLargeAction(actDisp);
    }
    tab->addPanel(defPanel);

    // Diagrammes 3D & Efforts (4 familles de sollicitations)
    auto* forcePanel = new RibbonPanel(QObject::tr("Diagrammes 3D"), tab);
    std::vector<QAction*> diagCol1;
    if (acts.actionDiagramMz) diagCol1.push_back(acts.actionDiagramMz);
    if (acts.actionDiagramMy) diagCol1.push_back(acts.actionDiagramMy);
    if (acts.actionDiagramMx) diagCol1.push_back(acts.actionDiagramMx);
    if (!diagCol1.empty()) forcePanel->addSmallColumn(diagCol1);

    std::vector<QAction*> diagCol2;
    if (acts.actionDiagramVz) diagCol2.push_back(acts.actionDiagramVz);
    if (acts.actionDiagramVy) diagCol2.push_back(acts.actionDiagramVy);
    if (acts.actionDiagramN) diagCol2.push_back(acts.actionDiagramN);
    if (!diagCol2.empty())
    {
        forcePanel->addInternalSeparator();
        forcePanel->addSmallColumn(diagCol2);
    }

    std::vector<QAction*> diagCol3;
    if (acts.actionDiagramDeflection) diagCol3.push_back(acts.actionDiagramDeflection);
    if (acts.actionDiagramNone) diagCol3.push_back(acts.actionDiagramNone);
    if (!diagCol3.empty())
    {
        forcePanel->addInternalSeparator();
        forcePanel->addSmallColumn(diagCol3);
    }
    tab->addPanel(forcePanel);

    // Réactions aux Appuis
    if (acts.actionReactionsToggle)
    {
        auto* reactPanel = new RibbonPanel(QObject::tr("Réactions"), tab);
        reactPanel->addLargeAction(acts.actionReactionsToggle);
        tab->addPanel(reactPanel);
    }

    // Cadrage Caméra Contextuel
    if (acts.actionFitDeformed || acts.actionFitResults || acts.actionFitModel || acts.actionFitAll)
    {
        auto* camPanel = new RibbonPanel(QObject::tr("Cadrage"), tab);
        if (acts.actionFitDeformed) camPanel->addLargeAction(acts.actionFitDeformed);
        std::vector<QAction*> camCol;
        if (acts.actionFitResults) camCol.push_back(acts.actionFitResults);
        if (acts.actionFitModel) camCol.push_back(acts.actionFitModel);
        if (acts.actionFitAll) camCol.push_back(acts.actionFitAll);
        if (!camCol.empty())
        {
            camPanel->addInternalSeparator();
            camPanel->addSmallColumn(camCol);
        }
        tab->addPanel(camPanel);
    }

    // Note de Calcul (NDC)
    auto* ndcPanel = new RibbonPanel(QObject::tr("Note de Calcul"), tab);
    auto* actNdc = acts.actionOpenNDC ? acts.actionOpenNDC : new QAction(QIcon(":/icons/ndc_report.svg"), QObject::tr("Note de Calcul"), parentWindow);
    ndcPanel->addLargeAction(actNdc);
    tab->addPanel(ndcPanel);

    return tab;
}

// -----------------------------------------------------------------------------
// 6. Onglet ÉDITION
// -----------------------------------------------------------------------------
RibbonTab* RibbonBuilder::buildEditTab(RibbonBar* bar, const RibbonActions& acts, QWidget* /*parentWindow*/)
{
    auto* tab = bar->addTab(QObject::tr("Modifier"));

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

    // Symétrie & Topologie
    std::vector<QAction*> topoCol;
    if (acts.actionSplitBars) topoCol.push_back(acts.actionSplitBars);
    if (acts.actionMergeNodes) topoCol.push_back(acts.actionMergeNodes);
    if (acts.actionCleanModel) topoCol.push_back(acts.actionCleanModel);
    if (acts.actionMirror || !topoCol.empty())
    {
        auto* topoPanel = new RibbonPanel(QObject::tr("Symétrie & Topologie"), tab);
        if (acts.actionMirror) topoPanel->addLargeAction(acts.actionMirror);
        if (!topoCol.empty())
        {
            topoPanel->addInternalSeparator();
            topoPanel->addSmallColumn(topoCol);
        }
        tab->addPanel(topoPanel);
    }

    // Outils de modification avancés (registre des outils) + mode de saisie
    if (!acts.advancedModifyTools.empty() || acts.actionToolInputMode)
    {
        auto* toolsPanel = new RibbonPanel(QObject::tr("Outils de modification"), tab);
        if (acts.actionToolInputMode) toolsPanel->addLargeAction(acts.actionToolInputMode);
        for (std::size_t i = 0; i < acts.advancedModifyTools.size(); i += 3)
        {
            toolsPanel->addInternalSeparator();
            std::vector<QAction*> col(acts.advancedModifyTools.begin() + i,
                                      acts.advancedModifyTools.begin() + std::min(i + 3, acts.advancedModifyTools.size()));
            toolsPanel->addSmallColumn(col);
        }
        tab->addPanel(toolsPanel);
    }

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

    // Projections & Orientations
    auto* projPanel = new RibbonPanel(QObject::tr("Projections"), tab);
    if (acts.actionView3D) projPanel->addLargeAction(acts.actionView3D);
    std::vector<QAction*> colViews1;
    if (acts.actionViewTop) colViews1.push_back(acts.actionViewTop);
    else if (acts.actionViewXY) colViews1.push_back(acts.actionViewXY);
    if (acts.actionViewFront) colViews1.push_back(acts.actionViewFront);
    else if (acts.actionViewXZ) colViews1.push_back(acts.actionViewXZ);
    if (acts.actionViewRight) colViews1.push_back(acts.actionViewRight);
    else if (acts.actionViewYZ) colViews1.push_back(acts.actionViewYZ);
    if (!colViews1.empty())
    {
        projPanel->addInternalSeparator();
        projPanel->addSmallColumn(colViews1);
    }
    std::vector<QAction*> colViews2;
    if (acts.actionViewIsometric) colViews2.push_back(acts.actionViewIsometric);
    if (acts.actionViewHome) colViews2.push_back(acts.actionViewHome);
    if (acts.actionViewLeft) colViews2.push_back(acts.actionViewLeft);
    if (!colViews2.empty())
    {
        projPanel->addInternalSeparator();
        projPanel->addSmallColumn(colViews2);
    }
    tab->addPanel(projPanel);

    // Navigation & Cadrage
    auto* navPanel = new RibbonPanel(QObject::tr("Navigation"), tab);
    if (acts.actionFitAll) navPanel->addLargeAction(acts.actionFitAll);

    std::vector<QAction*> colZoom1;
    if (acts.actionFitSelection) colZoom1.push_back(acts.actionFitSelection);
    if (acts.actionZoomWindow) colZoom1.push_back(acts.actionZoomWindow);
    if (acts.actionResetView) colZoom1.push_back(acts.actionResetView);
    if (!colZoom1.empty())
    {
        navPanel->addInternalSeparator();
        navPanel->addSmallColumn(colZoom1);
    }

    std::vector<QAction*> colZoom2;
    if (acts.actionZoomIn) colZoom2.push_back(acts.actionZoomIn);
    if (acts.actionZoomOut) colZoom2.push_back(acts.actionZoomOut);
    if (acts.actionPreviousView) colZoom2.push_back(acts.actionPreviousView);
    if (acts.actionNextView) colZoom2.push_back(acts.actionNextView);
    if (!colZoom2.empty())
    {
        navPanel->addInternalSeparator();
        navPanel->addSmallColumn(colZoom2);
    }
    tab->addPanel(navPanel);

    // Plans de Travail, Repères & Coupes
    auto* cutPanel = new RibbonPanel(QObject::tr("Plans & Coupes"), tab);
    if (acts.actionViewNormalToPlane) cutPanel->addLargeAction(acts.actionViewNormalToPlane);
    if (acts.actionCoordSystem)
    {
        cutPanel->addInternalSeparator();
        cutPanel->addLargeAction(acts.actionCoordSystem);
    }
    std::vector<QAction*> colPlans;
    if (acts.actionWorkPlaneXY) colPlans.push_back(acts.actionWorkPlaneXY);
    if (acts.actionWorkPlaneLevel) colPlans.push_back(acts.actionWorkPlaneLevel);
    if (acts.actionWorkPlaneXZ) colPlans.push_back(acts.actionWorkPlaneXZ);
    if (acts.actionWorkPlaneYZ) colPlans.push_back(acts.actionWorkPlaneYZ);
    if (!colPlans.empty())
    {
        cutPanel->addInternalSeparator();
        cutPanel->addSmallColumn(colPlans);
    }
    std::vector<QAction*> colOpt;
    if (acts.actionWorkPlaneCustom) colOpt.push_back(acts.actionWorkPlaneCustom);
    if (acts.actionWorkPlaneVisible) colOpt.push_back(acts.actionWorkPlaneVisible);
    if (!colOpt.empty())
    {
        cutPanel->addInternalSeparator();
        cutPanel->addSmallColumn(colOpt);
    }
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
    if (acts.actionObjectSnap) visCol2.push_back(acts.actionObjectSnap);
    if (acts.actionGridLabels) visCol2.push_back(acts.actionGridLabels);
    if (acts.actionFullScreen) visCol2.push_back(acts.actionFullScreen);
    if (!visCol2.empty())
    {
        visPanel->addInternalSeparator();
        visPanel->addSmallColumn(visCol2);
    }

    std::vector<QAction*> visCol3;
    if (acts.actionNodesVisible) visCol3.push_back(acts.actionNodesVisible);
    if (acts.actionNodeLabelsVisible) visCol3.push_back(acts.actionNodeLabelsVisible);
    if (!visCol3.empty())
    {
        visPanel->addInternalSeparator();
        visPanel->addSmallColumn(visCol3);
    }
    tab->addPanel(visPanel);

    // Fenêtres Docks
    auto* dockPanel = new RibbonPanel(QObject::tr("Fenêtres & Docks"), tab);
    std::vector<QAction*> dockCol1;
    if (acts.actionToggleModelTree) dockCol1.push_back(acts.actionToggleModelTree);
    if (acts.actionToggleProperties) dockCol1.push_back(acts.actionToggleProperties);
    if (acts.actionResultsDock) dockCol1.push_back(acts.actionResultsDock);
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

    // IA Co-Engineering
    if (acts.actionAIAssistant)
    {
        auto* aiPanel = new RibbonPanel(QObject::tr("IA Co-Engineering"), tab);
        aiPanel->addLargeAction(acts.actionAIAssistant);
        std::vector<QAction*> aiCol;
        if (acts.actionAIAnalyze) aiCol.push_back(acts.actionAIAnalyze);
        if (acts.actionAICheck) aiCol.push_back(acts.actionAICheck);
        if (acts.actionAIExplain) aiCol.push_back(acts.actionAIExplain);
        if (!aiCol.empty())
        {
            aiPanel->addInternalSeparator();
            aiPanel->addSmallColumn(aiCol);
        }
        if (acts.actionAIConfig)
        {
            aiPanel->addInternalSeparator();
            aiPanel->addSmallColumn({ acts.actionAIConfig });
        }
        tab->addPanel(aiPanel);
    }

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
