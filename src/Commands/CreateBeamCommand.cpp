#include "CreateBeamCommand.h"

namespace TSA::Commands
{

CreateBeamCommand::CreateBeamCommand(TSA::Model::Model& model,
                                     int startNodeId,
                                     int endNodeId,
                                     double width,
                                     double height,
                                     const std::string& name)
    : m_model(model)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_width(width)
    , m_height(height)
    , m_beamName(name)
{
}

bool CreateBeamCommand::execute()
{
    if (m_createdBeamId > 0)
    {
        return m_model.addBeamWithId(m_createdBeamId, m_startNodeId, m_endNodeId, m_width, m_height, m_beamName);
    }

    m_createdBeamId = m_model.addBeam(m_startNodeId, m_endNodeId, m_width, m_height, m_beamName);
    return m_createdBeamId > 0;
}

bool CreateBeamCommand::undo()
{
    if (m_createdBeamId <= 0)
        return false;

    return m_model.removeBeam(m_createdBeamId);
}

} // namespace TSA::Commands
