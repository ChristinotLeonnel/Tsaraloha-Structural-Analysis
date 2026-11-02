#include "test_common.h"
#include "UI/WindowManager/WindowManager.h"
#include "UI/WindowManager/ProjectLayoutState.h"
#include "UI/Common/WheelGuard.h"
#include "UI/Dialogs/PasswordPromptDialog.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QAbstractItemView>
#include <QComboBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QTabBar>
#include <QTabWidget>
#include "UI/Workspace/WorkspaceView.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
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

    // -------------------------------------------------------------------------
    // TEST 576 : disposition propre au projet (bloc UIST, docs/PROJECT_STATE_PLAN.md étape 3) — panneaux (binaire Qt et
    // listes lisibles), masquage temporaire, espace de travail, onglet du ruban, repli, version inconnue, préférence D2.
    // -------------------------------------------------------------------------
    {
        using namespace TSA::UI;
        QMainWindow win;
        win.resize(1024, 768);
        auto* model3d = new QLabel("vue 3D");
        auto* workspace = new WorkspaceView(model3d);
        win.setCentralWidget(workspace);
        auto makeDock = [&](const char* name, Qt::DockWidgetArea area) {
            auto* d = new QDockWidget(QString::fromLatin1(name), &win);
            d->setObjectName(QString::fromLatin1(name));
            win.addDockWidget(area, d);
            return d;
        };
        QDockWidget* tree = makeDock("TreeDock", Qt::LeftDockWidgetArea);
        QDockWidget* props = makeDock("PropsDock", Qt::RightDockWidgetArea);
        QDockWidget* console = makeDock("ConsoleDock", Qt::BottomDockWidgetArea);
        WindowManager wm(&win);
        wm.registerDock("tree", "Arbre", "Modélisation", tree, Qt::LeftDockWidgetArea, true);
        wm.registerDock("props", "Propriétés", "Général", props, Qt::RightDockWidgetArea, true);
        wm.registerDock("console", "Console", "Outils", console, Qt::BottomDockWidgetArea, true);
        QTabWidget ribbonWidget;   // comme le ruban : QTabWidget dont la barre d'onglets est passée (RibbonBar::tabBar)
        for (const char* t : { "Accueil", "Modélisation", "Résultats" }) ribbonWidget.addTab(new QWidget, QString::fromUtf8(t));
        QTabBar& ribbon = *ribbonWidget.tabBar();
        win.show();
        QCoreApplication::processEvents();

        // Disposition A : console fermée, arbre masqué temporairement (mode Blueprint), onglet « Résultats », partage vertical
        console->hide();
        tree->hide();
        ribbon.setCurrentIndex(2);
        workspace->setSplitOrientation(Qt::Vertical);
        QCoreApplication::processEvents();
        const QJsonObject a = captureProjectLayout(wm, workspace, &ribbon, { tree });
        TEST_CHECK(a.value("schema").toInt() == 1 && !a.value("docks").toString().isEmpty() &&
                       a.value("openDocks").toArray().contains("tree") && a.value("closedDocks").toArray().contains("console") &&
                       a.value("ribbonTab").toObject().value("title").toString() == "Résultats",
                   "Test 576: disposition relevée — panneau masqué temporairement enregistré ouvert, console fermée, onglet");
        TEST_CHECK(tree->isHidden(), "Test 576: relevé sans réafficher le panneau masqué temporairement");

        // Tout changer, puis appliquer A
        console->show();
        props->hide();
        ribbon.setCurrentIndex(0);
        workspace->setSplitOrientation(Qt::Horizontal);
        QCoreApplication::processEvents();
        const QStringList w = applyProjectLayout(wm, workspace, &ribbon, a);
        QCoreApplication::processEvents();
        TEST_CHECK(w.isEmpty(), "Test 576: application sans avertissement (" << w.join("; ").toStdString() << ")");
        TEST_CHECK(!tree->isHidden() && !props->isHidden() && console->isHidden(),
                   "Test 576: panneaux rétablis (arbre ouvert, propriétés ouvertes, console fermée)");
        TEST_CHECK(ribbonWidget.currentIndex() == 2 && workspace->splitOrientation() == Qt::Vertical,
                   "Test 576: onglet du ruban et orientation de l'espace de travail rétablis");

        // Onglets réordonnés depuis : retrouvé par son titre ; état binaire illisible : listes lisibles appliquées
        QTabWidget otherWidget;
        otherWidget.addTab(new QWidget, QStringLiteral("Résultats"));
        otherWidget.addTab(new QWidget, QStringLiteral("Accueil"));
        QTabBar& other = *otherWidget.tabBar();
        QJsonObject broken = a;
        broken["docks"] = QStringLiteral("AAAA");
        console->show();
        const QStringList w2 = applyProjectLayout(wm, nullptr, &other, broken);
        TEST_CHECK(otherWidget.currentIndex() == 0 && w2.size() == 1 && console->isHidden(),
                   "Test 576: onglet retrouvé par son titre ; état binaire refusé signalé, listes lisibles appliquées");

        // Panneau apparu depuis l'enregistrement : sa visibilité n'est pas touchée
        QDockWidget* fresh = makeDock("FreshDock", Qt::RightDockWidgetArea);
        wm.registerDock("fresh", "Nouveau", "Outils", fresh, Qt::RightDockWidgetArea, true);
        fresh->show();
        applyProjectLayout(wm, workspace, &ribbon, a);
        QCoreApplication::processEvents();
        TEST_CHECK(!fresh->isHidden(), "Test 576: panneau inconnu de la disposition enregistrée laissé tel quel");

        // Version inconnue : signalée, rien appliqué
        QJsonObject future = a;
        future["schema"] = 7;
        console->show();
        TEST_CHECK(applyProjectLayout(wm, workspace, &ribbon, future).size() == 1 && !console->isHidden(),
                   "Test 576: version inconnue — signalée, rien appliqué");

        // Préférence D2 (réglages isolés : la suite n'a pas d'identité de réglages)
        const QString savedOrg = QCoreApplication::organizationName(), savedApp = QCoreApplication::applicationName();
        QCoreApplication::setOrganizationName(QStringLiteral("TSA_TestSuite_Isolated"));
        QCoreApplication::setApplicationName(QStringLiteral("layout%1").arg(QCoreApplication::applicationPid()));
        const bool byDefault = WindowManager::restoresProjectLayout();
        WindowManager::setRestoresProjectLayout(false);
        const bool off = !WindowManager::restoresProjectLayout();
        QSettings().clear();
        QCoreApplication::setOrganizationName(savedOrg);
        QCoreApplication::setApplicationName(savedApp);
        TEST_CHECK(byDefault && off, "Test 576: préférence « Restaurer la disposition enregistrée dans le projet » — vraie par défaut, désactivable");
        std::cout << "[PASS] Test 576: Disposition propre au projet (bloc UIST) — panneaux, espace de travail, ruban, repli" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 582 : la molette ne modifie jamais un champ numérique ni une liste déroulante survolés (UI/Common/WheelGuard) ;
    // auparavant, faire défiler un panneau dont un champ passait sous le curseur changeait sa valeur (modification non
    // demandée). Le défilement passe à la zone qui contient le champ.
    // -------------------------------------------------------------------------
    {
        TSA::UI::WheelGuard::install();
        TSA::UI::WheelGuard::install();   // idempotent
        QScrollArea area;
        area.resize(300, 200);
        auto* content = new QWidget;
        auto* layout = new QVBoxLayout(content);
        auto* spin = new QSpinBox;
        spin->setRange(0, 100);
        spin->setValue(50);
        auto* dspin = new QDoubleSpinBox;
        dspin->setValue(2.5);
        auto* combo = new QComboBox;
        combo->addItems({ "IPE 200", "IPE 300", "HEA 200" });
        combo->setCurrentIndex(1);
        layout->addWidget(spin);
        layout->addWidget(dspin);
        layout->addWidget(combo);
        layout->addSpacing(1500);   // contenu plus haut que la zone : défilement possible
        area.setWidget(content);
        area.show();
        QCoreApplication::processEvents();
        spin->setFocus();   // même un champ en cours d'édition (focus au clic) ne change plus à la molette
        QCoreApplication::processEvents();

        auto wheelOn = [](QWidget* w) {
            const QPointF local(w->width() / 2.0, w->height() / 2.0);
            QWheelEvent ev(local, w->mapToGlobal(local), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(w, &ev);
            QCoreApplication::processEvents();
        };
        const int scroll0 = area.verticalScrollBar()->value();
        wheelOn(spin);
        wheelOn(spin->findChild<QLineEdit*>());   // champ texte interne du QSpinBox
        wheelOn(dspin);
        wheelOn(combo);
        TEST_CHECK(spin->value() == 50 && dspin->value() == 2.5 && combo->currentIndex() == 1,
                   "Test 582: molette sur les champs — aucune valeur modifiée (" << spin->value() << ", " << dspin->value() << ", "
                                                                                 << combo->currentIndex() << ")");
        TEST_CHECK(area.verticalScrollBar()->value() > scroll0, "Test 582: la zone qui contient les champs défile (" << scroll0 << " → "
                                                                                                                   << area.verticalScrollBar()->value() << ")");
        TEST_CHECK(spin->focusPolicy() == Qt::StrongFocus && combo->focusPolicy() != Qt::WheelFocus,
                   "Test 582: les champs ne prennent plus le focus à la molette");
        combo->showPopup();
        QCoreApplication::processEvents();
        QAbstractItemView* list = combo->view();
        TEST_CHECK(list && TSA::UI::WheelGuard::protectedField(list->viewport()) == nullptr,
                   "Test 582: liste d'une liste déroulante ouverte — défilement normal");
        combo->hidePopup();
        std::cout << "[PASS] Test 582: La molette ne modifie plus les champs survolés (défilement de la zone)" << std::endl;
        passed++;
    }
    // TEST 601 : saisie du mot de passe — masquée par défaut, case « Afficher le mot de passe », texte conservé
    {
        TSA::UI::PasswordPromptDialog dialog(QStringLiteral("Projet protégé"), QStringLiteral("Mot de passe :"));
        auto* field = dialog.findChild<QLineEdit*>();
        auto* show = dialog.findChild<QCheckBox*>();
        TEST_CHECK(field && show && field->echoMode() == QLineEdit::Password && !show->isChecked(), "Test 601: saisie masquée par défaut, case décochée");
        field->setText(QStringLiteral("Pont-été-2026"));
        show->setChecked(true);
        TEST_CHECK(field->echoMode() == QLineEdit::Normal && field->text() == QStringLiteral("Pont-été-2026"), "Test 601: case cochée — texte visible, inchangé");
        show->setChecked(false);
        TEST_CHECK(field->echoMode() == QLineEdit::Password && dialog.password() == QStringLiteral("Pont-été-2026"), "Test 601: case décochée — texte de nouveau masqué");
        auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        TEST_CHECK(ok && ok->isDefault() | ok->autoDefault(), "Test 601: Entrée valide la saisie");

        // Fenêtre de protection : la même case commande la saisie et la confirmation
        QWidget parent;
        auto* first = new QLineEdit(&parent);
        auto* second = new QLineEdit(&parent);
        first->setEchoMode(QLineEdit::Password);
        second->setEchoMode(QLineEdit::Password);
        auto* both = TSA::UI::PasswordPromptDialog::createVisibilityToggle({ first, second }, &parent);
        both->setChecked(true);
        TEST_CHECK(first->echoMode() == QLineEdit::Normal && second->echoMode() == QLineEdit::Normal, "Test 601: saisie et confirmation visibles ensemble");
        both->setChecked(false);
        TEST_CHECK(first->echoMode() == QLineEdit::Password && second->echoMode() == QLineEdit::Password, "Test 601: saisie et confirmation masquées ensemble");
        std::cout << "[PASS] Test 601: Saisie du mot de passe, case Afficher" << std::endl;
        passed++;
    }
    return true;
}
