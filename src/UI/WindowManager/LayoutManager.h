#pragma once

#include "WindowRegistry.h"
#include <QObject>
#include <QMainWindow>
#include <QMap>
#include <QByteArray>
#include <QStringList>

namespace TSA::UI
{

/**
 * @brief Gestionnaire de disposition, d'ancrage et de profils d'affichage pour TSA.
 */
class LayoutManager : public QObject
{
    Q_OBJECT

public:
    static constexpr int LayoutVersion = 1;

    explicit LayoutManager(QMainWindow* mainWindow, WindowRegistry* registry, QObject* parent = nullptr);
    ~LayoutManager() override = default;

    /**
     * @brief Restaure la disposition canonique et propre par défaut de TSA.
     */
    void resetLayout();

    /**
     * @brief Sauvegarde l'état d'ancrage actuel sous forme de QByteArray binaire Qt.
     */
    [[nodiscard]] QByteArray saveState() const;

    /**
     * @brief Restaure l'état d'ancrage depuis un QByteArray binaire Qt.
     */
    bool restoreState(const QByteArray& state);

    /**
     * @brief Sauvegarde la disposition courante et les profils dans QSettings.
     */
    void saveToSettings(const QString& group = "Layout");

    /**
     * @brief Restaure la disposition depuis QSettings (ou applique le layout par défaut si vierge).
     * @return true si une disposition valide a été restaurée, false sinon.
     */
    bool restoreFromSettings(const QString& group = "Layout");

    /**
     * @brief Enregistre la disposition actuelle dans un profil nommé.
     */
    bool saveProfile(const QString& name);

    /**
     * @brief Applique un profil de disposition par son nom.
     */
    bool applyProfile(const QString& name);

    /**
     * @brief Vérifie si un profil existe.
     */
    [[nodiscard]] bool hasProfile(const QString& name) const;

    /**
     * @brief Renvoie la liste de tous les profils disponibles.
     */
    [[nodiscard]] QStringList availableProfiles() const;

    /**
     * @brief Renvoie le nom du profil actuellement actif.
     */
    [[nodiscard]] QString currentProfile() const;

    /**
     * @brief Synchronise les états cochés de toutes les actions de fenêtre avec leur visibilité réelle.
     */
    void syncAllActionStates();

signals:
    void layoutReset();
    void layoutRestored();
    void layoutSaved();
    void profileApplied(const QString& name);
    void profileSaved(const QString& name);

private:
    void applyPresetProfile(const QString& name);

private:
    QMainWindow* m_mainWindow = nullptr;
    WindowRegistry* m_registry = nullptr;
    QMap<QString, QByteArray> m_profiles;
    QString m_currentProfile = "Modélisation";
};

} // namespace TSA::UI
