#pragma once

// Start Center : seul écran affiché au lancement de TSA (aucun viewport, aucun panneau de
// modélisation). Nouveau projet, ouverture d'un fichier et projets récents (aperçu du dernier état
// réel de chaque modèle, nom, chemin, date de dernière ouverture), avec recherche et tri.

#include <QDateTime>
#include <QFrame>
#include <QImage>
#include <QPointer>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QGraphicsDropShadowEffect;

namespace TSA::UI
{

class ProjectCard : public QFrame
{
    Q_OBJECT

public:
    static constexpr int kImageWidth = 288;
    static constexpr int kImageHeight = 162;

    ProjectCard(const QString& path, const QDateTime& lastOpened, QWidget* parent = nullptr);

    const QString& path() const { return m_path; }
    const QString& name() const { return m_name; }
    const QDateTime& lastOpened() const { return m_lastOpened; }
    const QDateTime& lastModified() const { return m_lastModified; }

    void setPreview(const QImage& image);
    void setSelected(bool selected);
    bool matches(const QString& filter) const;

signals:
    void clicked(ProjectCard* card);
    void openRequested(const QString& path);
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
    static QPixmap placeholder(const QString& format);

private:
    QString m_path;
    QString m_name;
    QDateTime m_lastOpened;
    QDateTime m_lastModified;
    QLabel* m_image = nullptr;
    QGraphicsDropShadowEffect* m_shadow = nullptr;
};

class StartCenter : public QWidget
{
    Q_OBJECT

public:
    explicit StartCenter(QWidget* parent = nullptr);

    /// Reconstruit les cartes (projets récents existants) et charge les aperçus en tâche de fond.
    void refresh();
    /// Remplace l'aperçu d'une carte (nouvelle capture du dernier état du modèle).
    void updatePreview(const QString& path, const QImage& image);

    /// Point d'extension produit : remplace le bloc d'identité et d'actions (logo, Nouveau, Ouvrir)
    /// par un panneau fourni, placé à gauche des projets récents (ex. colonne « laboratoire » de
    /// TSALab). Le panneau devient enfant du Start Center et gère lui-même son style.
    void setLaunchPanel(QWidget* panel);

signals:
    void newProjectRequested();
    void openDialogRequested();
    void openRequested(const QString& path);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void applyTheme(bool dark);
    void applyFilter();
    void relayoutCards();
    void loadPreviewsAsync();
    void selectCard(ProjectCard* card);
    void showCardMenu(const QString& path, const QPoint& globalPos);

private:
    QHBoxLayout* m_outer = nullptr;
    QWidget* m_identity = nullptr; // logo, titre, Nouveau / Ouvrir (masqué par setLaunchPanel)
    QWidget* m_launchPanel = nullptr;
    QLabel* m_logo = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_sort = nullptr;
    QLabel* m_count = nullptr;
    QPushButton* m_btnNew = nullptr;
    QPushButton* m_btnOpen = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_cardsHost = nullptr;
    QGridLayout* m_grid = nullptr;
    QLabel* m_empty = nullptr;
    QList<QPointer<ProjectCard>> m_cards;   // tous les projets récents
    QList<QPointer<ProjectCard>> m_visible; // filtrés et triés, dans l'ordre affiché
    QPointer<ProjectCard> m_selected;
    int m_columns = 0;
    quint64 m_generation = 0; // annule les chargements d'aperçus d'un affichage précédent
};

} // namespace TSA::UI
