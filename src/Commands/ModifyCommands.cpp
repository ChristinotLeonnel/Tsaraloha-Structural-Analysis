#include "ModifyCommands.h"
#include "../Coordinate/WorkPlaneManager.h"

namespace TSA::Commands
{

// -----------------------------------------------------------------------------
// MoveElementsCommand
// -----------------------------------------------------------------------------
MoveElementsCommand::MoveElementsCommand(TSA::Model::Model& model, const std::set<int>& nodeIds,
                                         double dx, double dy, double dz)
    : m_model(model), m_nodeIds(nodeIds), m_dx(dx), m_dy(dy), m_dz(dz)
{
}

bool MoveElementsCommand::execute()
{
    return m_model.moveNodes(m_nodeIds, m_dx, m_dy, m_dz);
}

bool MoveElementsCommand::undo()
{
    return m_model.moveNodes(m_nodeIds, -m_dx, -m_dy, -m_dz);
}

// -----------------------------------------------------------------------------
// RotateElementsCommand
// -----------------------------------------------------------------------------
RotateElementsCommand::RotateElementsCommand(TSA::Model::Model& model, const std::set<int>& nodeIds,
                                             double cx, double cy, double cz,
                                             double angleDeg,
                                             double ax, double ay, double az)
    : m_model(model), m_nodeIds(nodeIds)
    , m_cx(cx), m_cy(cy), m_cz(cz)
    , m_angleDeg(angleDeg)
    , m_ax(ax), m_ay(ay), m_az(az)
{
}

bool RotateElementsCommand::execute()
{
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    return m_model.rotateNodes(m_nodeIds, gp_Pnt(m_cx, m_cy, m_cz), gp_Dir(m_ax, m_ay, m_az), m_angleDeg * kDegToRad);
}

bool RotateElementsCommand::undo()
{
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    return m_model.rotateNodes(m_nodeIds, gp_Pnt(m_cx, m_cy, m_cz), gp_Dir(m_ax, m_ay, m_az), -m_angleDeg * kDegToRad);
}

// -----------------------------------------------------------------------------
// DeleteElementsCommand
// -----------------------------------------------------------------------------
DeleteElementsCommand::DeleteElementsCommand(TSA::Model::Model& model,
                                             const std::set<int>& nodes,
                                             const std::set<int>& beams,
                                             const std::set<int>& columns,
                                             const std::set<int>& slabs,
                                             const std::set<int>& walls,
                                             const std::set<int>& foundations,
                                             const std::set<int>& trussMembers,
                                             const std::set<int>& cables)
    : m_model(model)
    , m_nodes(nodes), m_beams(beams), m_columns(columns), m_slabs(slabs)
    , m_walls(walls), m_foundations(foundations), m_trussMembers(trussMembers)
    , m_cables(cables)
{
}

bool DeleteElementsCommand::execute()
{
    // Sauvegarder l'état exact du modèle avant suppression pour un undo sans perte
    m_snapshot = m_model.createSnapshot("Suppression d'éléments");
    m_hasSnapshot = true;

    for (int sId : m_slabs) m_model.removeSlab(sId);
    for (int wId : m_walls) m_model.removeWall(wId);
    for (int fId : m_foundations) m_model.removeFoundation(fId);
    for (int tId : m_trussMembers) m_model.removeTrussMember(tId);
    for (int bId : m_beams) m_model.removeBeam(bId);
    for (int cId : m_columns) m_model.removeColumn(cId);
    for (int cId : m_cables) m_model.removeCable(cId);
    for (int nId : m_nodes) m_model.removeNode(nId);

    return true;
}

bool DeleteElementsCommand::undo()
{
    if (!m_hasSnapshot) return false;
    m_model.restoreSnapshot(m_snapshot);
    return true;
}

// -----------------------------------------------------------------------------
// ModifyWorkPlaneCommand
// -----------------------------------------------------------------------------
ModifyWorkPlaneCommand::ModifyWorkPlaneCommand(int workPlaneId,
                                               const TSA::Coordinate::WorkPlane& oldWp,
                                               const TSA::Coordinate::WorkPlane& newWp,
                                               TSA::Coordinate::WorkPlaneManager* manager)
    : m_wpId(workPlaneId)
    , m_oldWp(oldWp)
    , m_newWp(newWp)
    , m_manager(manager)
{
    m_oldWp.setId(m_wpId);
    m_newWp.setId(m_wpId);
}

bool ModifyWorkPlaneCommand::execute()
{
    if (m_manager)
    {
        m_manager->updateWorkPlane(m_newWp);
    }
    return true;
}

bool ModifyWorkPlaneCommand::undo()
{
    if (m_manager)
    {
        m_manager->updateWorkPlane(m_oldWp);
    }
    return true;
}

std::string ModifyWorkPlaneCommand::name() const
{
    return "Modifier Plan de Travail (" + m_newWp.name() + ")";
}

} // namespace TSA::Commands
