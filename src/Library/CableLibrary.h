#pragma once

#include "../Model/Cable/CableDefinition.h"
#include <vector>
#include <string>
#include <optional>

namespace TSA::Library
{

/**
 * @brief Gestionnaire de bibliothèque de câbles, torons et éléments tendus pour TSA.
 */
class CableLibrary
{
public:
    static CableLibrary& instance();

    void initialize();
    void save();

    /// Liste de tous les câbles standard officiels
    const std::vector<TSA::Model::CableDefinition>& standardDefinitions() const { return m_standardDefinitions; }

    /// Liste des câbles personnalisés créés par l'utilisateur
    const std::vector<TSA::Model::CableDefinition>& customDefinitions() const { return m_customDefinitions; }

    /// Liste combinée
    std::vector<TSA::Model::CableDefinition> allDefinitions() const;

    /// Ajout / suppression de définitions personnalisées
    bool addCustomDefinition(const TSA::Model::CableDefinition& def);
    bool removeCustomDefinition(const std::string& name);

    /// Recherche
    const TSA::Model::CableDefinition* findByName(const std::string& name) const;
    const TSA::Model::CableDefinition* findById(const std::string& id) const;

    /// Filtrage multicritère
    std::vector<TSA::Model::CableDefinition> filter(
        std::optional<TSA::Model::CableType> type = std::nullopt,
        std::optional<TSA::Model::StandardCode> standard = std::nullopt,
        double minDiameter = 0.0,
        double maxDiameter = 10.0
    ) const;

    /// Export et Import JSON
    std::string exportToJson() const;
    bool importFromJson(const std::string& jsonString);

private:
    CableLibrary();
    ~CableLibrary() = default;

    std::vector<TSA::Model::CableDefinition> m_standardDefinitions;
    std::vector<TSA::Model::CableDefinition> m_customDefinitions;
    std::string m_storageFilePath;
};

} // namespace TSA::Library
