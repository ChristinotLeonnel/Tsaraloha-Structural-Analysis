#pragma once

#include "WindowItem.h"
#include <QObject>
#include <QHash>
#include <QList>
#include <QStringList>

namespace TSA::UI
{

/**
 * @brief Registre central des fenêtres et panneaux de l'application TSA.
 * Assure le stockage ordonné, la recherche par identifiant unique et la notification d'événements.
 */
class WindowRegistry : public QObject
{
    Q_OBJECT

public:
    explicit WindowRegistry(QObject* parent = nullptr);
    ~WindowRegistry() override = default;

    /**
     * @brief Enregistre une nouvelle fenêtre ou panneau.
     * @param info Détails descriptifs de la fenêtre.
     * @return true si l'enregistrement a réussi, false si l'ID est invalide ou déjà enregistré.
     */
    bool registerWindow(const WindowInfo& info);

    /**
     * @brief Désenregistre une fenêtre par son ID.
     */
    bool unregisterWindow(const QString& id);

    /**
     * @brief Recherche les informations d'une fenêtre par son ID.
     */
    [[nodiscard]] WindowInfo* findWindow(const QString& id);
    [[nodiscard]] const WindowInfo* findWindow(const QString& id) const;

    /**
     * @brief Vérifie si une fenêtre avec l'ID donné est enregistrée.
     */
    [[nodiscard]] bool hasWindow(const QString& id) const;

    /**
     * @brief Renvoie la liste ordonnée de toutes les fenêtres enregistrées.
     */
    [[nodiscard]] QList<WindowInfo> allWindows() const;

    /**
     * @brief Renvoie la liste de toutes les catégories logiques présentes.
     */
    [[nodiscard]] QStringList categories() const;

    /**
     * @brief Renvoie toutes les fenêtres appartenant à une catégorie donnée.
     */
    [[nodiscard]] QList<WindowInfo> windowsByCategory(const QString& category) const;

    /**
     * @brief Réinitialise et vide le registre.
     */
    void clear();

    /**
     * @brief Nombre total de fenêtres enregistrées.
     */
    [[nodiscard]] int count() const;

signals:
    void windowRegistered(const QString& id);
    void windowUnregistered(const QString& id);
    void windowVisibilityChanged(const QString& id, bool visible);
    void windowFloatingChanged(const QString& id, bool floating);

private:
    void setupDockConnections(const QString& id, QDockWidget* dock);

private:
    QHash<QString, WindowInfo> m_windows;
    QList<QString> m_order;
};

} // namespace TSA::UI
