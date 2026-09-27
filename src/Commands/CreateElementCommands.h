#pragma once

#include "ICommand.h"
#include "../Model/Model.h"
#include "../Model/Cable/CableTypes.h"
#include "../Model/Cable/CableDefinition.h"
#include <string>
#include <vector>

namespace TSA::Commands
{

/**
 * @brief Commande de création d'un Nœud structural.
 */
class CreateNodeCommand : public ICommand
{
public:
    CreateNodeCommand(TSA::Model::Model& model, double x, double y, double z,
                      const std::string& levelId = "", const std::string& name = "");

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer Nœud " + (m_createdId > 0 ? std::to_string(m_createdId) : ""); }
    CommandCategory category() const override { return CommandCategory::Create; }

    int createdNodeId() const noexcept { return m_createdId; }

private:
    TSA::Model::Model& m_model;
    double m_x, m_y, m_z;
    std::string m_levelId;
    std::string m_nodeName;
    int m_createdId = -1;
};

/**
 * @brief Commande de création d'un Poteau structural.
 */
class CreateColumnCommand : public ICommand
{
public:
    CreateColumnCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                        double width = 0.30, double height = 0.30, const std::string& name = "Poteau");

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer Poteau " + (m_createdId > 0 ? std::to_string(m_createdId) : ""); }
    CommandCategory category() const override { return CommandCategory::Create; }

    int createdColumnId() const noexcept { return m_createdId; }

private:
    TSA::Model::Model& m_model;
    int m_startNodeId, m_endNodeId;
    double m_width, m_height;
    std::string m_columnName;
    int m_createdId = -1;
};

/**
 * @brief Commande de création d'un Câble structural.
 */
class CreateCableCommand : public ICommand
{
public:
    CreateCableCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                       const TSA::Model::CableDefinition& definition,
                       const std::string& name = "Câble",
                       TSA::Model::CableGeometryMode mode = TSA::Model::CableGeometryMode::Straight,
                       double sag = 0.0);

    CreateCableCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                       TSA::Model::CableType type = TSA::Model::CableType::StayCable,
                       const std::string& name = "Câble",
                       TSA::Model::CableGeometryMode mode = TSA::Model::CableGeometryMode::Straight,
                       double sag = 0.0);

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer Câble " + (m_createdId > 0 ? std::to_string(m_createdId) : ""); }
    CommandCategory category() const override { return CommandCategory::Create; }

    int createdCableId() const noexcept { return m_createdId; }

private:
    TSA::Model::Model& m_model;
    int m_startNodeId, m_endNodeId;
    TSA::Model::CableDefinition m_definition;
    bool m_useDefinition = false;
    TSA::Model::CableType m_type = TSA::Model::CableType::StayCable;
    std::string m_cableName;
    TSA::Model::CableGeometryMode m_geomMode;
    double m_sag;
    int m_createdId = -1;
};

/**
 * @brief Commande de création d'une Dalle structurale.
 */
class CreateSlabCommand : public ICommand
{
public:
    CreateSlabCommand(TSA::Model::Model& model, const std::vector<int>& nodeIds,
                      double thickness = 0.20, const std::string& name = "Dalle",
                      TSA::Model::SlabType type = TSA::Model::SlabType::TwoWay);

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer Dalle " + (m_createdId > 0 ? std::to_string(m_createdId) : ""); }
    CommandCategory category() const override { return CommandCategory::Create; }

    int createdSlabId() const noexcept { return m_createdId; }

private:
    TSA::Model::Model& m_model;
    std::vector<int> m_nodeIds;
    double m_thickness;
    std::string m_slabName;
    TSA::Model::SlabType m_type;
    int m_createdId = -1;
};

/**
 * @brief Commande de création d'un Voile structural.
 */
class CreateWallCommand : public ICommand
{
public:
    CreateWallCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                       double height = 3.0, double thickness = 0.20, const std::string& name = "Voile");

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer Voile " + (m_createdId > 0 ? std::to_string(m_createdId) : ""); }
    CommandCategory category() const override { return CommandCategory::Create; }

    int createdWallId() const noexcept { return m_createdId; }

private:
    TSA::Model::Model& m_model;
    int m_startNodeId, m_endNodeId;
    double m_height, m_thickness;
    std::string m_wallName;
    int m_createdId = -1;
};

/**
 * @brief Commande de création d'une Fondation structurale.
 */
class CreateFoundationCommand : public ICommand
{
public:
    CreateFoundationCommand(TSA::Model::Model& model, int nodeId,
                            double widthA = 1.50, double lengthB = 1.50, double heightH = 0.50,
                            const std::string& name = "Semelle",
                            TSA::Model::FoundationType type = TSA::Model::FoundationType::IsolatedFooting);

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer Fondation " + (m_createdId > 0 ? std::to_string(m_createdId) : ""); }
    CommandCategory category() const override { return CommandCategory::Create; }

    int createdFoundationId() const noexcept { return m_createdId; }

private:
    TSA::Model::Model& m_model;
    int m_nodeId;
    double m_widthA, m_lengthB, m_heightH;
    std::string m_foundationName;
    TSA::Model::FoundationType m_type;
    int m_createdId = -1;
};

/**
 * @brief Commande de création d'une Barre de Treillis.
 */
class CreateTrussMemberCommand : public ICommand
{
public:
    CreateTrussMemberCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                             double diameter = 0.10, const std::string& name = "Treillis",
                             TSA::Model::TrussMemberRole role = TSA::Model::TrussMemberRole::Diagonal);

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer Treillis " + (m_createdId > 0 ? std::to_string(m_createdId) : ""); }
    CommandCategory category() const override { return CommandCategory::Create; }

    int createdMemberId() const noexcept { return m_createdId; }

private:
    TSA::Model::Model& m_model;
    int m_startNodeId, m_endNodeId;
    double m_diameter;
    std::string m_memberName;
    TSA::Model::TrussMemberRole m_role;
    int m_createdId = -1;
};

} // namespace TSA::Commands
