#pragma once

// Fenêtre unique du produit (TSA, TSALab) : barre de titre personnalisée + deux modes exclusifs.
//   StartCenter      : écran d'accueil seul (créé au lancement) ;
//   ProjectWorkspace : un ou plusieurs documents ouverts, un onglet chacun (comme dans un logiciel de CAO).
// Chaque document est un MainWindow complet et indépendant : modèle, historique, maillage, résultats, vue, état
// d'enregistrement (aucune donnée partagée entre projets, sauf le presse-papiers structurel, voulu partagé pour copier
// d'un projet à l'autre). Les commandes de la barre de titre agissent sur le document au premier plan.
// Cycle de vie : Start Center → Nouveau / Ouvrir → onglet → Fermer (dernier onglet → Start Center).

#include <QHash>
#include <QWidget>

#include <memory>

class MainWindow;
class QAction;
class QAbstractNativeEventFilter;
class QMenu;
class QShortcut;
class QStackedWidget;
class QTabBar;
class QVBoxLayout;

namespace TSA::UI
{

class StartCenter;
class TitleBar;

enum class ApplicationMode
{
    StartCenter,
    ProjectWorkspace
};

class AppShell : public QWidget
{
    Q_OBJECT

public:
    explicit AppShell(QWidget* parent = nullptr);
    ~AppShell() override;

    ApplicationMode mode() const { return m_mode; }

    void showStartCenter();
    void showProjectWorkspace();

    void createNewProject();
    void openProject();
    /// Ouvre un fichier projet du produit (projets récents, glisser-déposer, ligne de commande).
    bool openProjectFile(const QString& path);
    /// Ferme le document au premier plan (confirmation si modifié).
    bool closeProject();
    bool closeAllProjects();
    void saveAllProjects();
    bool isProjectOpen() const { return m_mode == ApplicationMode::ProjectWorkspace; }
    QList<MainWindow*> documents() const { return m_documents; }
    void activateDocument(MainWindow* document);
    bool closeDocument(MainWindow* document);

    StartCenter* startCenter() const { return m_startCenter; }
    /// Document au premier plan (nul si aucun projet n'est ouvert).
    MainWindow* workspace() const { return m_workspace; }

protected:
    /// Ctrl+Tab / Ctrl+Maj+Tab, Ctrl+Pg.Suiv / Ctrl+Pg.Préc : document suivant / précédent (filtre de l'application :
    /// Ctrl+Maj+Tab arrive en Key_Backtab, qu'un QShortcut ne reconnaît pas de façon fiable)
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
#ifdef _WIN32
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private:
    MainWindow* createWorkspace();
    MainWindow* documentFor(const QString& path) const;
    void refreshTabs();
    QString tabLabel(const MainWindow* document) const;
    void showTabMenu(const QPoint& pos);
    bool loadIntoWorkspace(const QString& path);
    void setMode(ApplicationMode mode);
    void updateTitle();
    void createActions();
    void bindToWorkspace(QAction* proxy, QAction* target);
    void syncProxyActions();
    void rebuildApplicationMenu();
    void restoreShellGeometry();
    void saveShellGeometry() const;

    // Cadre natif Windows sans barre de titre système (ancrage, animations, redimensionnement conservés).
    void applyNativeFrame();
    void installResizeBorderFilter();
    void updateMaximizedMargins();

private:
    ApplicationMode m_mode = ApplicationMode::StartCenter;
    QVBoxLayout* m_layout = nullptr;
    TitleBar* m_titleBar = nullptr;
    QStackedWidget* m_stack = nullptr;
    StartCenter* m_startCenter = nullptr;
    MainWindow* m_workspace = nullptr;          ///< document au premier plan
    QList<MainWindow*> m_documents;              ///< ordre des onglets
    QTabBar* m_docTabs = nullptr;
    QAction* m_actCloseAll = nullptr;
    QAction* m_actSaveAll = nullptr;

    QAction* m_actNew = nullptr;
    QAction* m_actOpen = nullptr;
    QAction* m_actSave = nullptr;
    QAction* m_actSaveAs = nullptr;
    QAction* m_actUndo = nullptr;
    QAction* m_actRedo = nullptr;
    QAction* m_actCloseProject = nullptr;
    QAction* m_actTheme = nullptr;
    QAction* m_actExit = nullptr;
    QMenu* m_recentMenu = nullptr;
    QList<QShortcut*> m_startCenterShortcuts;
    QHash<QAction*, QAction*> m_proxyTargets; // action de la barre de titre → action du workspace

    bool m_nativeFrameApplied = false;
    std::unique_ptr<QAbstractNativeEventFilter> m_resizeFilter;
    bool m_wasMaximizedBeforeFullScreen = false;
};

} // namespace TSA::UI
