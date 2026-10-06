#pragma once

// Barre de titre personnalisée de la fenêtre TSA (rangée unique, façon AutoCAD) :
//   gauche  : bouton d'application (logo TSA, menu) + accès rapide (Nouveau, Ouvrir, Enregistrer, Annuler, Rétablir)
//   centre  : titre « TSA — Projet.tsa »
//   droite  : autres commandes + Réduire / Agrandir-Restaurer / Fermer (toujours à l'extrême droite).
// Le déplacement, le double-clic, l'ancrage (snap) et le menu système restent natifs : la fenêtre
// (AppShell) interroge isCaptionArea() pour répondre HTCAPTION à Windows.

#include <QWidget>

class QAction;
class QHBoxLayout;
class QLabel;
class QMenu;
class QToolButton;

namespace TSA::UI
{

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget* parent = nullptr);

    QMenu* applicationMenu() const { return m_appMenu; }

    /// Bouton d'accès rapide lié à une action (icône, infobulle et état actif suivent l'action).
    QToolButton* addQuickAccess(QAction* action);
    void addQuickAccessSeparator();
    /// Commande placée à droite, avant les boutons de fenêtre.
    QToolButton* addTrailing(QAction* action);

    void setTitle(const QString& title);

    /// Vrai si le point (coordonnées locales) est une zone de déplacement (pas un bouton).
    bool isCaptionArea(const QPoint& localPos) const;

    /// Met à jour le glyphe Agrandir / Restaurer.
    void updateWindowState(Qt::WindowStates state);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void applyTheme(bool dark);
    void placeTitle();
    QToolButton* makeCommandButton(QAction* action);
    QToolButton* makeWindowButton(const QString& glyph, const QString& objectName, const QString& toolTip);

private:
    QWidget* m_left = nullptr;
    QWidget* m_right = nullptr;
    QHBoxLayout* m_leftLayout = nullptr;
    QHBoxLayout* m_trailingLayout = nullptr;
    QToolButton* m_appButton = nullptr;
    QMenu* m_appMenu = nullptr;
    QLabel* m_title = nullptr;
    QString m_fullTitle;
    QToolButton* m_btnMinimize = nullptr;
    QToolButton* m_btnMaximize = nullptr;
    QToolButton* m_btnClose = nullptr;
};

} // namespace TSA::UI
