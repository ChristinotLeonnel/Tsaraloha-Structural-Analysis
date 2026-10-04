#pragma once

// Page d'accueil « Projets récents » : cartes avec l'aperçu du dernier état réel de chaque modèle
// (capturé dans le viewport TSA), nom du projet et date de dernière modification.
// Partage la zone centrale avec l'unique viewport (QStackedWidget) : aucun second viewport.

#include <QFrame>
#include <QImage>
#include <QPointer>
#include <QWidget>

class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QGraphicsDropShadowEffect;

namespace TSA::UI
{

class ProjectCard : public QFrame
{
    Q_OBJECT

public:
    static constexpr int kImageWidth = 320;
    static constexpr int kImageHeight = 180;

    ProjectCard(const QString& path, const QString& title, const QString& subtitle, const QString& format, QWidget* parent = nullptr);

    const QString& path() const { return m_path; }
    void setPreview(const QImage& image);
    void setCurrent(bool current);

signals:
    void openRequested(const QString& path);
    void contextMenuRequested(const QString& path, const QPoint& globalPos);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setHovered(bool hovered);
    static QPixmap placeholder(const QString& format);

private:
    QString m_path;
    QString m_format;
    QLabel* m_image = nullptr;
    QLabel* m_badge = nullptr;
    QLabel* m_openHint = nullptr;
    QGraphicsDropShadowEffect* m_shadow = nullptr;
};

class StartPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartPage(QWidget* parent = nullptr);

    /// Reconstruit les cartes (projets récents existants) et charge les aperçus en tâche de fond.
    void refresh(const QString& currentProjectPath = QString(), bool hasOpenModel = false);
    /// Remplace l'aperçu d'une carte (après « Actualiser l'aperçu » ou une nouvelle capture).
    void updatePreview(const QString& path, const QImage& image);

signals:
    void openRequested(const QString& path);
    void newProjectRequested();
    void openDialogRequested();
    void continueRequested();
    void cardContextMenuRequested(const QString& path, const QPoint& globalPos);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void relayoutCards();
    void loadPreviewsAsync();

private:
    QLabel* m_subtitle = nullptr;
    QPushButton* m_btnContinue = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_cardsHost = nullptr;
    QGridLayout* m_grid = nullptr;
    QLabel* m_empty = nullptr;
    QList<QPointer<ProjectCard>> m_cards;
    int m_columns = 0;
    quint64 m_generation = 0; // annule les chargements d'aperçus d'un affichage précédent
};

} // namespace TSA::UI
