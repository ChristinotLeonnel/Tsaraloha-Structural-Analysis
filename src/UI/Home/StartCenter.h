#pragma once

// Start Center : écran d'accueil de TSA, organisé comme les logiciels BIM de référence : colonne de gauche (identité, actions
// principales, navigation) et zone centrale à quatre pages — Accueil (projets ouverts, projets récents), Projets (dossier de
// projets), Fichiers récents (liste détaillée), Ressources. Aucun viewport ni panneau de modélisation. Il réutilise la création
// et l'ouverture d'AppShell (signaux), RecentProjects et ModelPreviewCache ; revenir à l'accueil ne ferme aucun projet.

#include "../Common/ProjectCardGrid.h"

#include <QFrame>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QVector>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

namespace TSA::UI
{

class RecentFilesView;

/// Projet ouvert dans un onglet, présenté à l'accueil pour y revenir.
struct OpenDocumentEntry
{
    QString label;   ///< libellé de l'onglet
    QString path;    ///< vide : projet jamais enregistré
};

class StartCenter : public QWidget
{
    Q_OBJECT

public:
    enum class Page { Home, Projects, Recent, Resources };

    explicit StartCenter(QWidget* parent = nullptr);

    /// Reconstruit les pages (projets récents existants) et charge aperçus et résumés en tâche de fond.
    void refresh();
    /// Remplace l'aperçu d'un projet (nouvelle capture du dernier état du modèle).
    void updatePreview(const QString& path, const QImage& image);
    /// Projets ouverts dans les onglets (liste vide : section masquée).
    void setOpenDocuments(const QList<OpenDocumentEntry>& documents);
    void showPage(Page page);
    Page currentPage() const;

signals:
    void newProjectRequested();
    void openDialogRequested();
    void openRequested(const QString& path);
    void languageRequested();
    void openDocumentRequested(int index);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* createSideBar();
    QWidget* createHomePage();
    QWidget* createProjectsPage();
    QWidget* createRecentPage();
    QWidget* createResourcesPage();
    QToolButton* addNavigation(QVBoxLayout* layout, const QString& icon, const QString& text, Page page);
    void applyTheme(bool dark);
    void rebuildOpenRecentMenu();
    void refreshProjectsPage();
    void updateHomeState();
    void updateRecentState();
    void updateProjectsState();
    void loadDetailsAsync(const QStringList& paths);
    void chooseProjectsFolder();
    void forget(const QString& path);
    void openResource(const QString& id);
    void showCardMenu(const QString& path, const QPoint& globalPos);

private:
    QWidget* m_sideBar = nullptr;
    QLabel* m_logo = nullptr;
    QWidget* m_identity = nullptr;
    QStackedWidget* m_pages = nullptr;
    QButtonGroup* m_navigation = nullptr;
    QPushButton* m_btnNew = nullptr;
    QPushButton* m_btnOpen = nullptr;
    QToolButton* m_btnOpenRecent = nullptr;
    QToolButton* m_btnBrowse = nullptr;
    QPushButton* m_btnLanguage = nullptr;
    QMenu* m_recentMenu = nullptr;

    // Accueil
    QWidget* m_openSection = nullptr;
    QHBoxLayout* m_openList = nullptr;
    ProjectCardGrid* m_homeGrid = nullptr;
    QLabel* m_homeEmpty = nullptr;
    QLabel* m_homeCount = nullptr;
    QPushButton* m_homeShowAll = nullptr;

    // Projets
    QLabel* m_folderLabel = nullptr;
    QLineEdit* m_projectsSearch = nullptr;
    ProjectCardGrid* m_projectsGrid = nullptr;
    QLabel* m_projectsEmpty = nullptr;
    QString m_projectsFolder;

    // Fichiers récents
    QLineEdit* m_recentSearch = nullptr;
    QComboBox* m_recentSort = nullptr;
    QLabel* m_recentCount = nullptr;
    RecentFilesView* m_recentView = nullptr;
    QLabel* m_recentEmpty = nullptr;

    quint64 m_generation = 0;   ///< annule les chargements d'un affichage précédent
    QVector<ProjectCardGrid::Entry> m_recent;
};

} // namespace TSA::UI
