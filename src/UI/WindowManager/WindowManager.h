#pragma once

#include "WindowRegistry.h"
#include "LayoutManager.h"
#include <QObject>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <memory>

namespace TSA::UI
{

/**
 * @brief Gestionnaire centralisé de toutes les fenêtres, panneaux et docks de TSA.
 * Fournit une interface unifiée pour le contrôle de visibilité, l'ancrage, les profils
 * de disposition et la génération dynamique du menu "Fenêtres".
 */
class WindowManager : public QObject
{
    Q_OBJECT

public:
    explicit WindowManager(QMainWindow* mainWindow, QObject* parent = nullptr);
    ~WindowManager() override = default;

    /**
     * @brief Enregistre un panneau ou widget quelconque.
     */
    bool registerWindow(const QString& id,
                        const QString& title,
                        const QString& category,
                        QWidget* widget,
                        Qt::DockWidgetArea defaultArea = Qt::NoDockWidgetArea,
                        bool defaultVisible = true,
                        const QKeySequence& shortcut = QKeySequence(),
                        const QIcon& icon = QIcon());

    /**
     * @brief Enregistre un panneau ancrable (QDockWidget).
     */
    bool registerDock(const QString& id,
                      const QString& title,
                      const QString& category,
                      QDockWidget* dock,
                      Qt::DockWidgetArea defaultArea,
                      bool defaultVisible = true,
                      const QKeySequence& shortcut = QKeySequence(),
                      const QIcon& icon = QIcon());

    /**
     * @brief Désenregistre une fenêtre du gestionnaire.
     */
    bool unregisterWindow(const QString& id);

    /**
     * @brief Affiche la fenêtre désignée par son ID.
     */
    bool showWindow(const QString& id);

    /**
     * @brief Masque la fenêtre désignée par son ID.
     */
    bool hideWindow(const QString& id);

    /**
     * @brief Bascule l'affichage (affiche si masquée, masque si visible).
     */
    bool toggleWindow(const QString& id);

    /**
     * @brief Vérifie si la fenêtre est actuellement visible.
     */
    [[nodiscard]] bool isWindowVisible(const QString& id) const;

    /**
     * @brief Vérifie si le dock est actuellement détaché / flottant.
     */
    [[nodiscard]] bool isWindowFloating(const QString& id) const;

    /**
     * @brief Définit l'état flottant d'un dock.
     */
    bool setWindowFloating(const QString& id, bool floating);

    /**
     * @brief Donne le focus et active la fenêtre.
     */
    bool focusWindow(const QString& id);

    /**
     * @brief Recherche une fenêtre par son ID.
     */
    [[nodiscard]] WindowInfo* findWindow(const QString& id);
    [[nodiscard]] const WindowInfo* findWindow(const QString& id) const;

    /**
     * @brief Vérifie si une fenêtre est enregistrée avec cet ID.
     */
    [[nodiscard]] bool hasWindow(const QString& id) const;

    /**
     * @brief Renvoie toutes les fenêtres enregistrées.
     */
    [[nodiscard]] QList<WindowInfo> allWindows() const;

    /**
     * @brief Renvoie toutes les catégories disponibles.
     */
    [[nodiscard]] QStringList categories() const;

    /**
     * @brief Restaure la disposition canonique d'origine de TSA.
     */
    void resetLayout();

    /**
     * @brief Sauvegarde la disposition actuelle dans les paramètres système.
     */
    void saveLayout();

    /**
     * @brief Restaure la disposition enregistrée depuis les paramètres système.
     */
    bool restoreLayout();

    /**
     * @brief Applique un profil de disposition spécifique.
     */
    bool applyProfile(const QString& name);

    /**
     * @brief Enregistre la disposition actuelle dans un profil nommé.
     */
    bool saveProfile(const QString& name);

    /**
     * @brief Liste de tous les profils disponibles.
     */
    [[nodiscard]] QStringList availableProfiles() const;

    /**
     * @brief Nom du profil actif.
     */
    [[nodiscard]] QString currentProfile() const;

    /**
     * @brief Construit ou actualise dynamiquement le menu "Fenêtres".
     */
    void populateWindowsMenu(QMenu* menu);

    /**
     * @brief Crée et ajoute le menu "Fenêtres" dans la barre de menu principale.
     */
    QMenu* createWindowsMenu(QMenuBar* menuBar);

    [[nodiscard]] WindowRegistry& registry() noexcept { return *m_registry; }
    [[nodiscard]] const WindowRegistry& registry() const noexcept { return *m_registry; }

    [[nodiscard]] LayoutManager& layoutManager() noexcept { return *m_layoutManager; }
    [[nodiscard]] const LayoutManager& layoutManager() const noexcept { return *m_layoutManager; }

    [[nodiscard]] QMainWindow* mainWindow() const noexcept { return m_mainWindow; }

signals:
    void windowRegistered(const QString& id);
    void windowUnregistered(const QString& id);
    void windowVisibilityChanged(const QString& id, bool visible);
    void windowFloatingChanged(const QString& id, bool floating);
    void layoutReset();
    void layoutRestored();
    void layoutSaved();
    void profileApplied(const QString& name);

private:
    QMainWindow* m_mainWindow = nullptr;
    std::unique_ptr<WindowRegistry> m_registry;
    std::unique_ptr<LayoutManager> m_layoutManager;
};

} // namespace TSA::UI
