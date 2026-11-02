#pragma once

// Carte d'un projet et grille de cartes de l'accueil (pages Accueil et Projets du Start Center). La grille suit la largeur
// disponible, filtre et trie ; elle ne lit ni ne modifie la liste des projets récents : ses actions sont des signaux.

#include "../../Project/ProjectFileSummary.h"

#include <QDateTime>
#include <QFrame>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QVector>
#include <QWidget>

class QGraphicsDropShadowEffect;
class QGridLayout;
class QLabel;
class QScrollArea;
class QToolButton;

namespace TSA::UI
{

enum class ProjectSortKey { LastOpened, Name, Modified };

/// Image recadrée au centre pour remplir le cadre sans déformation.
QPixmap cropToFill(const QImage& image, int width, int height);

/// Texte du type de projet (« Projet TSA 3.2 », « Projet TSA 3.2 · protégé »).
QString projectTypeText(const TSA::Project::ProjectFileSummary& summary);

class ProjectCard : public QFrame
{
    Q_OBJECT

public:
    static constexpr int kImageWidth = 288;
    static constexpr int kImageHeight = 162;

    /// `removable` : bouton « Retirer de la liste » (projets récents).
    ProjectCard(const QString& path, const QDateTime& lastOpened, bool removable, QWidget* parent = nullptr);

    const QString& path() const { return m_path; }
    const QString& name() const { return m_name; }
    const QDateTime& lastOpened() const { return m_lastOpened; }
    const QDateTime& lastModified() const { return m_lastModified; }

    void setPreview(const QImage& image);
    void setSummary(const TSA::Project::ProjectFileSummary& summary);
    void setSelected(bool selected);
    bool matches(const QString& filter) const;

signals:
    void clicked(ProjectCard* card);
    void openRequested(const QString& path);
    void removeRequested(const QString& path);
    void contextMenuRequested(const QString& path, const QPoint& globalPos);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setStateProperty(const char* name, bool value);
    void setElided(QLabel* label, const QString& text, Qt::TextElideMode mode);
    static QPixmap placeholder(const QString& text);

private:
    QString m_path;
    QString m_name;
    QDateTime m_lastOpened;
    QDateTime m_lastModified;
    QLabel* m_image = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_type = nullptr;
    bool m_hasPreview = false;
    QGraphicsDropShadowEffect* m_shadow = nullptr;
};

class ProjectCardGrid : public QWidget
{
    Q_OBJECT

public:
    struct Entry
    {
        QString path;
        QDateTime lastOpened;
    };

    explicit ProjectCardGrid(QWidget* parent = nullptr);

    /// Remplace les cartes. `limit` > 0 : seules les `limit` premières sont affichées (page Accueil).
    void setProjects(const QVector<Entry>& entries, bool removable, int limit = 0);
    void setFilter(const QString& filter);
    void setSortKey(ProjectSortKey key);

    int count() const { return m_cards.size(); }
    int visibleCount() const { return m_visible.size(); }

    void updatePreview(const QString& path, const QImage& image);
    void updateSummary(const QString& path, const TSA::Project::ProjectFileSummary& summary);

signals:
    void openRequested(const QString& path);
    void removeRequested(const QString& path);
    void contextMenuRequested(const QString& path, const QPoint& globalPos);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void applyView();
    void relayout();
    void select(ProjectCard* card);

private:
    QScrollArea* m_scroll = nullptr;
    QWidget* m_host = nullptr;
    QGridLayout* m_grid = nullptr;
    QList<QPointer<ProjectCard>> m_cards;
    QList<QPointer<ProjectCard>> m_visible;
    QPointer<ProjectCard> m_selected;
    QString m_filter;
    ProjectSortKey m_sort = ProjectSortKey::LastOpened;
    int m_limit = 0;
    int m_columns = 0;
};

} // namespace TSA::UI
