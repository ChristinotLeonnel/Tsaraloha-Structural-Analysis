#pragma once

#include "ICommand.h"
#include "../Model/Model.h"
#include "../Coordinate/WorkPlane.h"
#include <set>
#include <vector>
#include <string>

namespace TSA::Coordinate { class WorkPlaneManager; }

namespace TSA::Commands
{

/**
 * @brief Commande de déplacement (Translation 3D) d'un ensemble de nœuds / éléments.
 */
class MoveElementsCommand : public ICommand
{
public:
    MoveElementsCommand(TSA::Model::Model& model, const std::set<int>& nodeIds,
                        double dx, double dy, double dz);

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Déplacer éléments (" + std::to_string(m_nodeIds.size()) + " nœuds)"; }
    CommandCategory category() const override { return CommandCategory::Modify; }

private:
    TSA::Model::Model& m_model;
    std::set<int> m_nodeIds;
    double m_dx, m_dy, m_dz;
};

/**
 * @brief Commande de rotation 3D d'un ensemble de nœuds / éléments autour d'un axe.
 */
class RotateElementsCommand : public ICommand
{
public:
    RotateElementsCommand(TSA::Model::Model& model, const std::set<int>& nodeIds,
                          double cx, double cy, double cz,
                          double angleDeg,
                          double ax = 0.0, double ay = 0.0, double az = 1.0);

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Rotation 3D (" + std::to_string(m_nodeIds.size()) + " nœuds)"; }
    CommandCategory category() const override { return CommandCategory::Modify; }

private:
    TSA::Model::Model& m_model;
    std::set<int> m_nodeIds;
    double m_cx, m_cy, m_cz;
    double m_angleDeg;
    double m_ax, m_ay, m_az;
};

/**
 * @brief Commande de suppression d'éléments du modèle avec historique d'annulation.
 */
class DeleteElementsCommand : public ICommand
{
public:
    DeleteElementsCommand(TSA::Model::Model& model,
                          const std::set<int>& nodes,
                          const std::set<int>& beams,
                          const std::set<int>& columns,
                          const std::set<int>& slabs,
                          const std::set<int>& walls,
                          const std::set<int>& foundations,
                          const std::set<int>& trussMembers,
                          const std::set<int>& cables);

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Suppression d'éléments"; }
    CommandCategory category() const override { return CommandCategory::Modify; }

private:
    TSA::Model::Model& m_model;
    std::set<int> m_nodes;
    std::set<int> m_beams;
    std::set<int> m_columns;
    std::set<int> m_slabs;
    std::set<int> m_walls;
    std::set<int> m_foundations;
    std::set<int> m_trussMembers;
    std::set<int> m_cables;

    // Snapshot Memento local pour garantir un undo bit-exact parfait
    TSA::Model::Model::ModelStateSnapshot m_snapshot;
    bool m_hasSnapshot = false;
};

/**
 * @brief Commande de modification géométrique ou paramétrique d'un plan de travail.
 * Gère l'annulation/rétablissement instantané (Undo/Redo) sans reconstruire toute la scène 3D.
 */
class ModifyWorkPlaneCommand : public ICommand
{
public:
    ModifyWorkPlaneCommand(int workPlaneId,
                           const TSA::Coordinate::WorkPlane& oldWp,
                           const TSA::Coordinate::WorkPlane& newWp,
                           TSA::Coordinate::WorkPlaneManager* manager = nullptr);

    bool execute() override;
    bool undo() override;
    std::string name() const override;
    CommandCategory category() const override { return CommandCategory::Modify; }

private:
    int m_wpId = 1;
    TSA::Coordinate::WorkPlane m_oldWp;
    TSA::Coordinate::WorkPlane m_newWp;
    TSA::Coordinate::WorkPlaneManager* m_manager = nullptr;
};

} // namespace TSA::Commands
