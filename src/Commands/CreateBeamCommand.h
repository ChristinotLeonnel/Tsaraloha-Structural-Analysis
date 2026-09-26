#pragma once

#include "ICommand.h"
#include "../Model/Model.h"

namespace TSA::Commands
{

/**
 * @brief Commande de création d'une poutre (Beam) dans le modèle.
 */
class CreateBeamCommand : public ICommand
{
public:
    CreateBeamCommand(TSA::Model::Model& model,
                      int startNodeId,
                      int endNodeId,
                      double width = 0.30,
                      double height = 0.50,
                      const std::string& name = "Poutre");

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer " + m_beamName; }

    int createdBeamId() const noexcept { return m_createdBeamId; }

private:
    TSA::Model::Model& m_model;
    int m_startNodeId = 0;
    int m_endNodeId = 0;
    double m_width = 0.30;
    double m_height = 0.50;
    std::string m_beamName;
    int m_createdBeamId = -1;
};

} // namespace TSA::Commands
