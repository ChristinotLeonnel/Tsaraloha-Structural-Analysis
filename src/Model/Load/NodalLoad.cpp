#include "NodalLoad.h"

namespace TSA::Model
{

NodalLoad::NodalLoad(int id, int nodeId, int loadCaseId,
                     double fx, double fy, double fz,
                     double mx, double my, double mz,
                     LoadCoordSystem coordSys,
                     const std::string& name)
    : m_id(id)
    , m_nodeId(nodeId)
    , m_loadCaseId(loadCaseId)
    , m_fx(fx)
    , m_fy(fy)
    , m_fz(fz)
    , m_mx(mx)
    , m_my(my)
    , m_mz(mz)
    , m_coordSys(coordSys)
    , m_name(name)
{
    if (m_name.empty() && m_id > 0)
    {
        m_name = "NL" + std::to_string(m_id);
    }
}

} // namespace TSA::Model
