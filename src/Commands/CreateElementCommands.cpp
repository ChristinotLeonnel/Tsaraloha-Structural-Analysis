#include "CreateElementCommands.h"

namespace TSA::Commands
{

// -----------------------------------------------------------------------------
// CreateNodeCommand
// -----------------------------------------------------------------------------
CreateNodeCommand::CreateNodeCommand(TSA::Model::Model& model, double x, double y, double z,
                                     const std::string& levelId, const std::string& name)
    : m_model(model), m_x(x), m_y(y), m_z(z), m_levelId(levelId), m_nodeName(name)
{
}

bool CreateNodeCommand::execute()
{
    if (m_createdId > 0)
    {
        return m_model.addNodeWithId(m_createdId, m_x, m_y, m_z, m_levelId, m_nodeName);
    }
    m_createdId = m_model.addNode(m_x, m_y, m_z, m_levelId, m_nodeName);
    return m_createdId > 0;
}

bool CreateNodeCommand::undo()
{
    if (m_createdId <= 0) return false;
    return m_model.removeNode(m_createdId);
}

// -----------------------------------------------------------------------------
// CreateColumnCommand
// -----------------------------------------------------------------------------
CreateColumnCommand::CreateColumnCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                                         double width, double height, const std::string& name)
    : m_model(model), m_startNodeId(startNodeId), m_endNodeId(endNodeId)
    , m_width(width), m_height(height), m_columnName(name)
{
}

bool CreateColumnCommand::execute()
{
    if (m_createdId > 0)
    {
        return m_model.addColumnWithId(m_createdId, m_startNodeId, m_endNodeId, m_width, m_height, m_columnName);
    }
    m_createdId = m_model.addColumn(m_startNodeId, m_endNodeId, m_width, m_height, m_columnName);
    return m_createdId > 0;
}

bool CreateColumnCommand::undo()
{
    if (m_createdId <= 0) return false;
    return m_model.removeColumn(m_createdId);
}

// -----------------------------------------------------------------------------
// CreateCableCommand
// -----------------------------------------------------------------------------
CreateCableCommand::CreateCableCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                                       const TSA::Model::CableDefinition& definition,
                                       const std::string& name,
                                       TSA::Model::CableGeometryMode mode,
                                       double sag)
    : m_model(model), m_startNodeId(startNodeId), m_endNodeId(endNodeId)
    , m_definition(definition), m_useDefinition(true), m_cableName(name)
    , m_geomMode(mode), m_sag(sag)
{
}

CreateCableCommand::CreateCableCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                                       TSA::Model::CableType type,
                                       const std::string& name,
                                       TSA::Model::CableGeometryMode mode,
                                       double sag)
    : m_model(model), m_startNodeId(startNodeId), m_endNodeId(endNodeId)
    , m_useDefinition(false), m_type(type), m_cableName(name)
    , m_geomMode(mode), m_sag(sag)
{
}

bool CreateCableCommand::execute()
{
    if (m_createdId > 0)
    {
        if (m_useDefinition)
        {
            return m_model.addCableWithId(m_createdId, m_startNodeId, m_endNodeId, m_definition, m_cableName, m_geomMode, m_sag);
        }
        else
        {
            TSA::Model::CableDefinition def(m_cableName, m_type);
            return m_model.addCableWithId(m_createdId, m_startNodeId, m_endNodeId, def, m_cableName, m_geomMode, m_sag);
        }
    }

    if (m_useDefinition)
    {
        m_createdId = m_model.addCable(m_startNodeId, m_endNodeId, m_definition, m_cableName, m_geomMode, m_sag);
    }
    else
    {
        m_createdId = m_model.addCable(m_startNodeId, m_endNodeId, m_type, m_cableName, m_geomMode, m_sag);
    }
    return m_createdId > 0;
}

bool CreateCableCommand::undo()
{
    if (m_createdId <= 0) return false;
    return m_model.removeCable(m_createdId);
}

// -----------------------------------------------------------------------------
// CreateSlabCommand
// -----------------------------------------------------------------------------
CreateSlabCommand::CreateSlabCommand(TSA::Model::Model& model, const std::vector<int>& nodeIds,
                                     double thickness, const std::string& name,
                                     TSA::Model::SlabType type)
    : m_model(model), m_nodeIds(nodeIds), m_thickness(thickness), m_slabName(name), m_type(type)
{
}

bool CreateSlabCommand::execute()
{
    if (m_createdId > 0)
    {
        return m_model.addSlabWithId(m_createdId, m_nodeIds, m_thickness, m_slabName, m_type);
    }
    m_createdId = m_model.addSlab(m_nodeIds, m_thickness, m_slabName, m_type);
    return m_createdId > 0;
}

bool CreateSlabCommand::undo()
{
    if (m_createdId <= 0) return false;
    return m_model.removeSlab(m_createdId);
}

// -----------------------------------------------------------------------------
// CreateWallCommand
// -----------------------------------------------------------------------------
CreateWallCommand::CreateWallCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                                       double height, double thickness, const std::string& name)
    : m_model(model), m_startNodeId(startNodeId), m_endNodeId(endNodeId)
    , m_height(height), m_thickness(thickness), m_wallName(name)
{
}

bool CreateWallCommand::execute()
{
    if (m_createdId > 0)
    {
        return m_model.addWallWithId(m_createdId, m_startNodeId, m_endNodeId, m_height, m_thickness, m_wallName);
    }
    m_createdId = m_model.addWall(m_startNodeId, m_endNodeId, m_height, m_thickness, m_wallName);
    return m_createdId > 0;
}

bool CreateWallCommand::undo()
{
    if (m_createdId <= 0) return false;
    return m_model.removeWall(m_createdId);
}

// -----------------------------------------------------------------------------
// CreateFoundationCommand
// -----------------------------------------------------------------------------
CreateFoundationCommand::CreateFoundationCommand(TSA::Model::Model& model, int nodeId,
                                                 double widthA, double lengthB, double heightH,
                                                 const std::string& name,
                                                 TSA::Model::FoundationType type)
    : m_model(model), m_nodeId(nodeId), m_widthA(widthA), m_lengthB(lengthB), m_heightH(heightH)
    , m_foundationName(name), m_type(type)
{
}

bool CreateFoundationCommand::execute()
{
    if (m_createdId > 0)
    {
        return m_model.addFoundationWithId(m_createdId, m_nodeId, m_widthA, m_lengthB, m_heightH, m_foundationName, m_type);
    }
    m_createdId = m_model.addFoundation(m_nodeId, m_widthA, m_lengthB, m_heightH, m_foundationName, m_type);
    return m_createdId > 0;
}

bool CreateFoundationCommand::undo()
{
    if (m_createdId <= 0) return false;
    return m_model.removeFoundation(m_createdId);
}

// -----------------------------------------------------------------------------
// CreateTrussMemberCommand
// -----------------------------------------------------------------------------
CreateTrussMemberCommand::CreateTrussMemberCommand(TSA::Model::Model& model, int startNodeId, int endNodeId,
                                                   double diameter, const std::string& name,
                                                   TSA::Model::TrussMemberRole role)
    : m_model(model), m_startNodeId(startNodeId), m_endNodeId(endNodeId)
    , m_diameter(diameter), m_memberName(name), m_role(role)
{
}

bool CreateTrussMemberCommand::execute()
{
    if (m_createdId > 0)
    {
        return m_model.addTrussMemberWithId(m_createdId, m_startNodeId, m_endNodeId, m_diameter, m_memberName, m_role);
    }
    m_createdId = m_model.addTrussMember(m_startNodeId, m_endNodeId, m_diameter, m_memberName, m_role);
    return m_createdId > 0;
}

bool CreateTrussMemberCommand::undo()
{
    if (m_createdId <= 0) return false;
    return m_model.removeTrussMember(m_createdId);
}

} // namespace TSA::Commands
