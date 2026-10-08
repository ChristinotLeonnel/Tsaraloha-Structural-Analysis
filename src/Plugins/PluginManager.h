#pragma once

// Chargement des plugins (côté application, couche modèle : Qt Core seulement).
// Les commandes et nœuds des plugins sont ajoutés aux registres globaux (CommandRegistry::global,
// NodeLibrary::global) : console, Blueprint et IA les voient sans autre code. Les DLL ne sont jamais
// déchargées (leurs fonctions restent référencées par les registres).

#include "PluginApi.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace TSA::Plugins
{

struct LoadedPlugin
{
    QString path;
    PluginInfo info;
    bool loaded = false;
    QString error;
    std::vector<std::string> commands;   ///< commandes ajoutées
    std::vector<std::string> nodes;      ///< nœuds ajoutés (hors nœuds des commandes)
    QStringList log;                     ///< messages du plugin (IPluginHost::log)
};

class PluginManager
{
public:
    static PluginManager& instance();

    /// Charge chaque DLL du dossier (une seule fois par fichier). Rend le nombre de plugins chargés.
    int loadDirectory(const QString& directory);
    /// Charge une DLL ; faux avec *error (API incompatible, point d'entrée absent, initialisation refusée).
    bool loadFile(const QString& path, QString* error = nullptr);

    const std::vector<LoadedPlugin>& plugins() const { return m_plugins; }
    /// Dossier standard : <dossier de l'application>/plugins.
    static QString defaultDirectory();

private:
    PluginManager() = default;
    std::vector<LoadedPlugin> m_plugins;
    std::vector<std::unique_ptr<IPlugin>> m_instances;
};

} // namespace TSA::Plugins
