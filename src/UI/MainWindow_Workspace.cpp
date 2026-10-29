// Espace de travail central (docs/UI.md, « Espace de travail ») : dispositions Modèle 3D / Blueprint / Partagé de
// WorkspaceView, commandes du Blueprint dans le ruban, panneaux contextuels, mode focus, disposition par défaut et
// préférences (réglages de l'application, jamais le projet).
#include "MainWindow.h"

#include "Blueprint/BlueprintEditor.h"
#include "Dock/LogConsoleDock.h"
#include "Theme/ThemedIcon.h"
#include "WindowManager/WindowManager.h"
#include "Workspace/WorkspaceView.h"

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QFileDialog>
#include <QSettings>

void MainWindow::createWorkspaceActions()
{
    using Mode = TSA::UI::WorkspaceView::Mode;
    m_workspaceModeGroup = new QActionGroup(this);
    m_workspaceModeGroup->setExclusive(true);
    auto modeAction = [this](const QString& text, const QString& icon, const QString& tip, Mode mode) {
        auto* a = new QAction(TSA::UI::themedIcon(icon), text, this);
        a->setCheckable(true);
        a->setToolTip(tip);
        a->setActionGroup(m_workspaceModeGroup);
        connect(a, &QAction::triggered, this, [this, mode] { m_workspace->setMode(mode); });
        return a;
    };
    m_actionWorkspace3D = modeAction(tr("Modèle 3D"), QStringLiteral(":/icons/workspace/model3d.svg"),
                                     tr("Espace de travail : la vue 3D du modèle seule (Ctrl+Alt+1)"), Mode::Model3D);
    m_actionWorkspaceBlueprint = modeAction(tr("Blueprint"), QStringLiteral(":/icons/workspace/blueprint.svg"),
                                            tr("Espace de travail : l'éditeur de Blueprint seul, toute la surface centrale (Ctrl+Alt+2)"),
                                            Mode::Blueprint);
    m_actionWorkspaceSplit = modeAction(tr("Vue partagée"), QStringLiteral(":/icons/workspace/split.svg"),
                                        tr("Espace de travail : vue 3D et Blueprint côte à côte, redimensionnables (Ctrl+Alt+3)"), Mode::Split);
    m_actionWorkspace3D->setChecked(true);

    m_actionWorkspaceVertical = new QAction(tr("Partage vertical"), this);
    m_actionWorkspaceVertical->setCheckable(true);
    m_actionWorkspaceVertical->setToolTip(tr("Vue partagée : la vue 3D au-dessus du Blueprint plutôt qu'à côté"));
    connect(m_actionWorkspaceVertical, &QAction::toggled, this, [this](bool vertical) {
        m_workspace->setSplitOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
    });

    m_actionFocusMode = new QAction(TSA::UI::themedIcon(QStringLiteral(":/icons/workspace/focus.svg")), tr("Mode focus"), this);
    m_actionFocusMode->setCheckable(true);
    m_actionFocusMode->setToolTip(tr("Masquer tous les panneaux pour agrandir l'espace de travail ; les rétablir ensuite (Ctrl+Maj+F)"));
    connect(m_actionFocusMode, &QAction::toggled, this, &MainWindow::setFocusMode);

    m_actionResetLayout = new QAction(TSA::UI::themedIcon(QStringLiteral(":/icons/workspace/reset_layout.svg")), tr("Disposition par défaut"), this);
    m_actionResetLayout->setToolTip(tr("Rétablir la disposition par défaut des panneaux et de l'espace de travail"));
    connect(m_actionResetLayout, &QAction::triggered, this, &MainWindow::resetWorkspaceLayout);

    // Blueprint : chaque commande ouvre l'éditeur s'il n'est pas visible (mode Blueprint depuis le mode 3D)
    auto blueprintAction = [this](const QString& text, const QString& icon, const QString& tip, auto&& run) {
        auto* a = icon.isEmpty() ? new QAction(text, this) : new QAction(TSA::UI::themedIcon(icon), text, this);
        a->setToolTip(tip);
        connect(a, &QAction::triggered, this, [this, run] {
            if (m_workspace->mode() == TSA::UI::WorkspaceView::Mode::Model3D) m_workspace->setMode(TSA::UI::WorkspaceView::Mode::Blueprint);
            run(m_workspace->blueprint());
        });
        return a;
    };
    using Editor = TSA::UI::BlueprintEditor;
    m_actionBlueprintNew = blueprintAction(tr("Nouveau Blueprint"), QStringLiteral(":/icons/blueprint/new.svg"),
                                           tr("Nouveau programme de modélisation en blocs, exécuté sur ce projet"), [](Editor* e) { e->newBlueprint(); });
    m_actionBlueprintOpen = blueprintAction(tr("Ouvrir un Blueprint…"), QStringLiteral(":/icons/blueprint/open.svg"), tr("Ouvrir un fichier Blueprint (.tsbp)"),
                                            [this](Editor* e) {
                                                const QString path = QFileDialog::getOpenFileName(this, tr("Ouvrir un Blueprint"), e->currentFile(),
                                                                                                  tr("Blueprint (*.tsbp)"));
                                                if (!path.isEmpty()) e->open(path);
                                            });
    m_actionBlueprintSave = blueprintAction(tr("Enregistrer le Blueprint"), QStringLiteral(":/icons/blueprint/save.svg"),
                                            tr("Enregistrer le Blueprint édité (fichier .tsbp, distinct du projet)"), [](Editor* e) { e->save(); });
    m_actionBlueprintValidate = blueprintAction(tr("Valider"), QString(), tr("Contrôler le graphe sans l'exécuter (onglet Problèmes de l'inspecteur)"),
                                                [](Editor* e) { e->validate(); });
    m_actionBlueprintRun = blueprintAction(tr("Exécuter"), QStringLiteral(":/icons/blueprint/run.svg"),
                                           tr("Exécuter le Blueprint sur le projet ouvert (une entrée Annuler par commande)"), [](Editor* e) { e->run(); });
    m_actionBlueprintDebug = blueprintAction(tr("Déboguer"), QString(), tr("Exécuter en s'arrêtant aux points d'arrêt"), [](Editor* e) { e->debug(); });
    m_actionBlueprintFitAll = blueprintAction(tr("Cadrer le graphe"), QStringLiteral(":/icons/blueprint/fit_all.svg"), tr("Afficher tout le graphe"),
                                              [](Editor* e) { e->fitAll(); });
    m_actionBlueprintFitSelection = blueprintAction(tr("Cadrer la sélection du graphe"), QStringLiteral(":/icons/blueprint/fit_selection.svg"),
                                                    tr("Cadrer les nœuds sélectionnés du Blueprint"), [](Editor* e) { e->fitSelection(); });

    connect(m_workspace, &TSA::UI::WorkspaceView::modeChanged, this, &MainWindow::onWorkspaceModeChanged);
    connect(m_workspace, &TSA::UI::WorkspaceView::blueprintCreated, this, &MainWindow::connectBlueprintEditor);
}

void MainWindow::connectBlueprintEditor(TSA::UI::BlueprintEditor* editor)
{
    connect(editor, &TSA::UI::BlueprintEditor::logMessage, this, [this](const QString& text, const QString& type) {
        if (m_consoleDock) m_consoleDock->appendLog(text, type);
    });
    // Le Blueprint passe par le registre central : modèle, historique et vues sont déjà à jour ; la fenêtre rafraîchit
    // ses actions Annuler / Rétablir et son titre (document modifié).
    connect(editor, &TSA::UI::BlueprintEditor::projectModified, this, [this] {
        updateUndoRedoActions();
        updateWindowTitle();
    });
}

void MainWindow::onWorkspaceModeChanged()
{
    using Mode = TSA::UI::WorkspaceView::Mode;
    const Mode mode = m_workspace->mode();
    QAction* checked = mode == Mode::Blueprint ? m_actionWorkspaceBlueprint : mode == Mode::Split ? m_actionWorkspaceSplit : m_actionWorkspace3D;
    if (checked && !checked->isChecked()) checked->setChecked(true);
    applyContextualDocks();
}

void MainWindow::setFocusMode(bool on)
{
    applyContextualDocks();
    if (on && m_consoleDock) m_consoleDock->appendLog(tr("Mode focus : panneaux masqués (Ctrl+Maj+F pour les rétablir)."), "SYS");
}

void MainWindow::applyContextualDocks()
{
    // Panneaux contextuels, recalculés à chaque changement de mode ou du mode focus (quel que soit l'ordre) :
    //  - mode focus : tous les panneaux ancrés masqués ;
    //  - mode Blueprint : l'éditeur porte sa palette (explorateur) et son inspecteur ; les panneaux du modèle 3D
    //    s'effacent, la console (journal du Blueprint) reste ;
    //  - sinon : chaque panneau masqué ici revient à l'identique.
    // Seuls les panneaux masqués par l'espace de travail sont rétablis ; les panneaux flottants ne sont jamais touchés.
    // isHidden() (état explicite) et non isVisible() : à la restauration des préférences la fenêtre n'est pas encore
    // affichée, et un panneau en onglet inactif n'est pas visible sans être fermé.
    const bool focus = m_actionFocusMode && m_actionFocusMode->isChecked();
    const bool blueprint = m_workspace && m_workspace->mode() == TSA::UI::WorkspaceView::Mode::Blueprint;
    auto shouldHide = [&](QDockWidget* dock) { return focus || (blueprint && dock != m_consoleDock); };
    QList<QDockWidget*> restored;
    for (auto it = m_docksHiddenByWorkspace.begin(); it != m_docksHiddenByWorkspace.end();)
    {
        if (!*it) it = m_docksHiddenByWorkspace.erase(it);
        else if (!shouldHide(*it))
        {
            (*it)->show();
            restored << *it;
            it = m_docksHiddenByWorkspace.erase(it);
        }
        else ++it;
    }
    // Panneaux en onglets : chaque réaffichage change l'onglet actif ; l'onglet affiché avant le masquage revient devant
    for (QDockWidget* dock : std::as_const(restored))
        if (m_docksOnTopBeforeHide.contains(dock)) dock->raise();
    m_docksOnTopBeforeHide.removeIf([&](const QPointer<QDockWidget>& d) { return !d || restored.contains(d.data()); });
    QList<QDockWidget*> toHide;
    for (QDockWidget* dock : findChildren<QDockWidget*>())
        if (!dock->isHidden() && !dock->isFloating() && shouldHide(dock)) toHide << dock;
    // Onglets actifs relevés AVANT tout masquage : masquer l'onglet actif d'un groupe amène aussitôt le suivant devant
    // (constaté dans TSA.exe : « Calques » remplacé par « Arbre du modèle » au retour du mode Blueprint).
    for (QDockWidget* dock : std::as_const(toHide))
        if (dock->isVisible()) m_docksOnTopBeforeHide << dock;   // fenêtre affichée : onglet actif de son groupe
    for (QDockWidget* dock : std::as_const(toHide))
    {
        m_docksHiddenByWorkspace << dock;
        dock->hide();
    }
}

void MainWindow::restoreContextualDocks()
{
    // Avant d'enregistrer la disposition des panneaux : jamais un masquage temporaire (Blueprint, focus) dans l'état sauvé.
    for (const QPointer<QDockWidget>& dock : std::as_const(m_docksHiddenByWorkspace))
        if (dock) dock->show();
    for (const QPointer<QDockWidget>& dock : std::as_const(m_docksOnTopBeforeHide))
        if (dock) dock->raise();
    m_docksHiddenByWorkspace.clear();
    m_docksOnTopBeforeHide.clear();
    if (m_actionFocusMode && m_actionFocusMode->isChecked())
    {
        QSignalBlocker block(m_actionFocusMode);
        m_actionFocusMode->setChecked(false);
    }
}

void MainWindow::resetWorkspaceLayout()
{
    restoreContextualDocks();
    if (m_windowManager) m_windowManager->resetLayout();
    {
        QSignalBlocker block(m_actionWorkspaceVertical);
        m_actionWorkspaceVertical->setChecked(false);
    }
    m_workspace->resetLayout();
    if (m_consoleDock) m_consoleDock->appendLog(tr("Disposition par défaut rétablie."), "SYS");
}

void MainWindow::saveWorkspaceState()
{
    QSettings settings;
    m_workspace->saveState(settings);
}

void MainWindow::restoreWorkspaceState()
{
    QSettings settings;
    m_workspace->restoreState(settings);
    QSignalBlocker block(m_actionWorkspaceVertical);
    m_actionWorkspaceVertical->setChecked(m_workspace->splitOrientation() == Qt::Vertical);
    onWorkspaceModeChanged();
}
