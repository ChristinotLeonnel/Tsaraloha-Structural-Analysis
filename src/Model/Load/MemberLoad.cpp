#include "MemberLoad.h"

namespace TSA::Model
{

MemberLoad::MemberLoad(int id, int elementId, int loadCaseId,
                       LoadType type,
                       double q1, double q2,
                       LoadDirection direction,
                       LoadCoordSystem coordSys,
                       double x1, double x2,
                       bool isRelative,
                       const std::string& name,
                       MemberTargetType targetType)
    : m_id(id)
    , m_elementId(elementId)
    , m_loadCaseId(loadCaseId)
    , m_type(type)
    , m_q1(q1)
    , m_q2(q2)
    , m_direction(direction)
    , m_coordSys(coordSys)
    , m_x1(x1)
    , m_x2(x2)
    , m_isRelative(isRelative)
    , m_name(name)
    , m_targetType(targetType)
{
    if (m_name.empty() && m_id > 0)
    {
        m_name = "ML" + std::to_string(m_id);
    }
}

MemberLoad MemberLoad::uniform(int id, int elementId, int loadCaseId,
                              double q, LoadDirection dir,
                              LoadCoordSystem sys,
                              const std::string& name,
                              MemberTargetType targetType)
{
    return MemberLoad(id, elementId, loadCaseId, LoadType::MemberUniform, q, q, dir, sys, 0.0, 0.0, false, name, targetType);
}

MemberLoad MemberLoad::trapezoidal(int id, int elementId, int loadCaseId,
                                  double q1, double q2, double x1, double x2,
                                  LoadDirection dir,
                                  LoadCoordSystem sys,
                                  bool isRelative,
                                  const std::string& name)
{
    return MemberLoad(id, elementId, loadCaseId, LoadType::MemberLinear, q1, q2, dir, sys, x1, x2, isRelative, name);
}

MemberLoad MemberLoad::pointOnMember(int id, int elementId, int loadCaseId,
                                     double p, double position,
                                     LoadDirection dir,
                                     LoadCoordSystem sys,
                                     bool isRelative,
                                     const std::string& name)
{
    return MemberLoad(id, elementId, loadCaseId, LoadType::MemberPoint, p, p, dir, sys, position, position, isRelative, name);
}

} // namespace TSA::Model
