#pragma once

#include <QString>
#include <QPointer>
#include <QWidget>
#include <QDockWidget>
#include <QKeySequence>
#include <QIcon>
#include <QAction>
#include <QPoint>
#include <QSize>

namespace TSA::UI
{

/**
 * @brief Informations descriptives et état d'une fenêtre ou d'un panneau géré par le WindowManager.
 */
struct WindowInfo
{
    QString id;                             ///< Identifiant unique et immuable (ex. "model_browser", "properties")
    QString title;                          ///< Nom d'affichage pour l'utilisateur
    QString category;                       ///< Catégorie logique (ex. "Modélisation", "Affichage", "Outils")
    QPointer<QWidget> widget = nullptr;     ///< Widget associé (central ou panneau quelconque)
    QPointer<QDockWidget> dockWidget = nullptr; ///< Pointeur vers le QDockWidget si c'est un panneau ancrable
    QKeySequence shortcut;                  ///< Raccourci clavier facultatif
    QIcon icon;                             ///< Icône associée
    Qt::DockWidgetArea defaultArea = Qt::NoDockWidgetArea; ///< Zone d'ancrage initiale recommandée
    bool defaultVisible = true;             ///< Visibilité par défaut lors d'une réinitialisation
    bool defaultFloating = false;           ///< État flottant par défaut
    QPointer<QAction> action = nullptr;     ///< Action de menu / toggle liée

    [[nodiscard]] bool isDock() const noexcept { return dockWidget != nullptr; }
    [[nodiscard]] bool isVisible() const;
    [[nodiscard]] bool isFloating() const;
    [[nodiscard]] QPoint pos() const;
    [[nodiscard]] QSize size() const;
};

} // namespace TSA::UI
