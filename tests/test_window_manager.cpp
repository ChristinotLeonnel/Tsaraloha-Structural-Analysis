#include "test_common.h"
#include "UI/WindowManager/WindowManager.h"
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>

using namespace TSA::UI;

bool runSuite_WindowManager(int& passed)
{
    std::cout << "\n--- TEST 54: System Centralise WindowManager, Docks & Layout Profiles ---" << std::endl;

    // Fenêtre principale de test
    QMainWindow mainWindow;
    mainWindow.resize(1024, 768);

    auto* centralWidget = new QLabel("Viewport 3D Central", &mainWindow);
    mainWindow.setCentralWidget(centralWidget);

    // Création de 4 docks de test
    auto* dockModel = new QDockWidget("Navigateur du modèle", &mainWindow);
    dockModel->setObjectName("ModelTreeDock");
    mainWindow.addDockWidget(Qt::LeftDockWidgetArea, dockModel);

    auto* dockProperties = new QDockWidget("Propriétés", &mainWindow);
    dockProperties->setObjectName("PropertiesDock");
    mainWindow.addDockWidget(Qt::RightDockWidgetArea, dockProperties);

    auto* dockConsole = new QDockWidget("Console & Messages", &mainWindow);
    dockConsole->setObjectName("LogConsoleDock");
    mainWindow.addDockWidget(Qt::BottomDockWidgetArea, dockConsole);

    auto* dockElements = new QDockWidget("Éléments", &mainWindow);
    dockElements->setObjectName("StructuralElementsDock");
    mainWindow.addDockWidget(Qt::LeftDockWidgetArea, dockElements);

    // Initialisation du WindowManager
    WindowManager winMgr(&mainWindow);
    mainWindow.show();

    // -------------------------------------------------------------------------
    // Subtest 54.1: Enregistrement des fenêtres et docks
    // -------------------------------------------------------------------------
    bool regCentral = winMgr.registerWindow("viewport", "Vue 3D", "Général", centralWidget,
                                           Qt::NoDockWidgetArea, true, QKeySequence("Ctrl+1"));
    bool regModel = winMgr.registerDock("model_browser", "Navigateur du modèle", "Modélisation", dockModel,
                                        Qt::LeftDockWidgetArea, true, QKeySequence("Ctrl+3"));
    bool regProp = winMgr.registerDock("properties", "Propriétés", "Général", dockProperties,
                                       Qt::RightDockWidgetArea, true, QKeySequence("Ctrl+2"));
    bool regConsole = winMgr.registerDock("console", "Console & Messages", "Outils", dockConsole,
                                         Qt::BottomDockWidgetArea, true, QKeySequence("F2"));
    bool regElements = winMgr.registerDock("elements", "Éléments", "Modélisation", dockElements,
                                          Qt::LeftDockWidgetArea, true, QKeySequence("Ctrl+5"));

    TEST_CHECK(regCentral && regModel && regProp && regConsole && regElements,
               "Subtest 54.1: Enregistrement initial des 5 fenêtres/docks");
    TEST_CHECK(winMgr.allWindows().size() == 5,
               "Subtest 54.1: Nombre de fenêtres enregistrées == 5");

    // -------------------------------------------------------------------------
    // Subtest 54.2: Recherche par ID et détection d'ID inexistant
    // -------------------------------------------------------------------------
    TEST_CHECK(winMgr.hasWindow("model_browser"), "Subtest 54.2: hasWindow('model_browser') == true");
    TEST_CHECK(winMgr.hasWindow("properties"), "Subtest 54.2: hasWindow('properties') == true");
    TEST_CHECK(!winMgr.hasWindow("non_existent_window"), "Subtest 54.2: hasWindow inexistant == false");
    TEST_CHECK(winMgr.findWindow("non_existent_window") == nullptr, "Subtest 54.2: findWindow inexistant renvoie nullptr");

    auto* item = winMgr.findWindow("properties");
    TEST_CHECK(item != nullptr && item->title == "Propriétés", "Subtest 54.2: findWindow('properties') valide");
    TEST_CHECK(item->shortcut == QKeySequence("Ctrl+2"), "Subtest 54.2: Raccourci Ctrl+2 conservé");

    // -------------------------------------------------------------------------
    // Subtest 54.3: Affichage, Masquage, Toggle et synchronisation
    // -------------------------------------------------------------------------
    winMgr.hideWindow("properties");
    TEST_CHECK(!winMgr.isWindowVisible("properties"), "Subtest 54.3: hideWindow('properties') rend invisible");
    TEST_CHECK(dockProperties->isHidden(), "Subtest 54.3: QDockWidget properties est masqué");
    if (item->action)
    {
        TEST_CHECK(!item->action->isChecked(), "Subtest 54.3: QAction décochée après hide");
    }

    winMgr.showWindow("properties");
    TEST_CHECK(winMgr.isWindowVisible("properties"), "Subtest 54.3: showWindow('properties') rend visible");
    TEST_CHECK(!dockProperties->isHidden(), "Subtest 54.3: QDockWidget properties est affiché");
    if (item->action)
    {
        TEST_CHECK(item->action->isChecked(), "Subtest 54.3: QAction cochée après show");
    }

    winMgr.toggleWindow("properties");
    TEST_CHECK(!winMgr.isWindowVisible("properties"), "Subtest 54.3: toggleWindow bascule à masqué");
    winMgr.toggleWindow("properties");
    TEST_CHECK(winMgr.isWindowVisible("properties"), "Subtest 54.3: toggleWindow bascule à visible");

    // Fermeture directe du dock par l'utilisateur (fermeture native Qt)
    dockProperties->hide();
    TEST_CHECK(!winMgr.isWindowVisible("properties"), "Subtest 54.3: Fermeture native du dock répercutée");

    // -------------------------------------------------------------------------
    // Subtest 54.4: Fenêtres flottantes (Floating state)
    // -------------------------------------------------------------------------
    TEST_CHECK(!winMgr.isWindowFloating("console"), "Subtest 54.4: Console initialement non-flottante");
    winMgr.setWindowFloating("console", true);
    TEST_CHECK(winMgr.isWindowFloating("console"), "Subtest 54.4: Console rendue flottante");
    TEST_CHECK(dockConsole->isFloating(), "Subtest 54.4: QDockWidget::isFloating() vérifié");
    winMgr.setWindowFloating("console", false);
    TEST_CHECK(!winMgr.isWindowFloating("console"), "Subtest 54.4: Console rattachée (non-flottante)");

    // -------------------------------------------------------------------------
    // Subtest 54.5: Réinitialisation de la disposition (Reset Layout)
    // -------------------------------------------------------------------------
    // Altérer la disposition : masquer des docks, détacher
    winMgr.hideWindow("model_browser");
    winMgr.setWindowFloating("console", true);

    winMgr.resetLayout();
    TEST_CHECK(winMgr.isWindowVisible("model_browser"), "Subtest 54.5: ModelTree restauré visible après reset");
    TEST_CHECK(winMgr.isWindowVisible("properties"), "Subtest 54.5: Properties restauré visible après reset");
    TEST_CHECK(!winMgr.isWindowFloating("console"), "Subtest 54.5: Console rattachée après reset");
    TEST_CHECK(mainWindow.dockWidgetArea(dockConsole) == Qt::BottomDockWidgetArea,
               "Subtest 54.5: Console ancrée au bas après reset");

    // -------------------------------------------------------------------------
    // Subtest 54.6: Sauvegarde et Restauration du Layout (Roundtrip binaire)
    // -------------------------------------------------------------------------
    winMgr.hideWindow("elements");
    QByteArray savedState = winMgr.layoutManager().saveState();
    TEST_CHECK(!savedState.isEmpty(), "Subtest 54.6: saveState génère un QByteArray non vide");

    // Modifier l'état courant
    winMgr.showWindow("elements");
    TEST_CHECK(winMgr.isWindowVisible("elements"), "Subtest 54.6: elements rendu visible");

    // Restaurer l'état
    bool restored = winMgr.layoutManager().restoreState(savedState);
    TEST_CHECK(restored, "Subtest 54.6: restoreState réussit");
    TEST_CHECK(!winMgr.isWindowVisible("elements"), "Subtest 54.6: elements restauré masqué conforme");

    // -------------------------------------------------------------------------
    // Subtest 54.7: Profils de disposition (Modélisation, Analyse, Résultats...)
    // -------------------------------------------------------------------------
    const QStringList profiles = winMgr.availableProfiles();
    TEST_CHECK(profiles.contains("Modélisation") &&
               profiles.contains("Analyse") &&
               profiles.contains("Résultats") &&
               profiles.contains("Détaillage") &&
               profiles.contains("Personnalisée"),
               "Subtest 54.7: Profils standards disponibles");

    bool appAnalyse = winMgr.applyProfile("Analyse");
    TEST_CHECK(appAnalyse, "Subtest 54.7: Application profil 'Analyse'");
    TEST_CHECK(winMgr.currentProfile() == "Analyse", "Subtest 54.7: Profil courant == Analyse");

    bool appResultats = winMgr.applyProfile("Résultats");
    TEST_CHECK(appResultats, "Subtest 54.7: Application profil 'Résultats'");
    TEST_CHECK(winMgr.currentProfile() == "Résultats", "Subtest 54.7: Profil courant == Résultats");

    bool saveCustom = winMgr.saveProfile("Personnalisée");
    TEST_CHECK(saveCustom, "Subtest 54.7: Sauvegarde profil 'Personnalisée'");

    // -------------------------------------------------------------------------
    // Subtest 54.8: Génération dynamique du menu "Fenêtres"
    // -------------------------------------------------------------------------
    QMenu menuWindows;
    winMgr.populateWindowsMenu(&menuWindows);

    TEST_CHECK(!menuWindows.actions().isEmpty(), "Subtest 54.8: Menu 'Fenêtres' peuplé d'actions");

    // Vérifier la présence d'une action pour chaque dock
    bool foundModelAct = false;
    bool foundPropAct = false;
    bool foundLayoutSub = false;
    bool foundResetAct = false;

    for (auto* act : menuWindows.actions())
    {
        if (act->text().contains("Navigateur")) foundModelAct = true;
        if (act->text().contains("Propriétés")) foundPropAct = true;
        if (act->menu() != nullptr && act->text().contains("Disposition")) foundLayoutSub = true;
        if (act->text().contains("Réinitialiser")) foundResetAct = true;
    }

    TEST_CHECK(foundModelAct, "Subtest 54.8: Action 'Navigateur' présente dans le menu");
    TEST_CHECK(foundPropAct, "Subtest 54.8: Action 'Propriétés' présente dans le menu");
    TEST_CHECK(foundLayoutSub, "Subtest 54.8: Sous-menu 'Disposition' présent dans le menu");
    TEST_CHECK(foundResetAct, "Subtest 54.8: Action 'Réinitialiser la disposition' présente");

    // -------------------------------------------------------------------------
    // Subtest 54.9: Désenregistrement et nettoyage
    // -------------------------------------------------------------------------
    bool unreg = winMgr.unregisterWindow("elements");
    TEST_CHECK(unreg, "Subtest 54.9: unregisterWindow('elements') réussit");
    TEST_CHECK(!winMgr.hasWindow("elements"), "Subtest 54.9: elements n'est plus dans le registre");
    TEST_CHECK(winMgr.allWindows().size() == 4, "Subtest 54.9: Nombre total == 4 après retrait");

    std::cout << "[PASS] Test 54: Centralized WindowManager, Docking & Layout Profiles Passed Successfully!" << std::endl;
    passed++;
    return true;
}
