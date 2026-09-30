#pragma once

#include "LoadEnums.h"
#include <string>
#include <cmath>

namespace TSA::Model
{

/**
 * @brief Représente une charge nodale (forces Fx, Fy, Fz et moments Mx, My, Mz)
 * appliquée directement à un nœud du modèle structural TSA.
 */
class NodalLoad
{
public:
    NodalLoad() = default;
    NodalLoad(int id, int nodeId, int loadCaseId,
              double fx = 0.0, double fy = 0.0, double fz = 0.0,
              double mx = 0.0, double my = 0.0, double mz = 0.0,
              LoadCoordSystem coordSys = LoadCoordSystem::Global,
              const std::string& name = "");

    // Identifiants
    int id() const noexcept { return m_id; }
    void setId(int id) noexcept { m_id = id; }

    int nodeId() const noexcept { return m_nodeId; }
    void setNodeId(int nodeId) noexcept { m_nodeId = nodeId; }

    int loadCaseId() const noexcept { return m_loadCaseId; }
    void setLoadCaseId(int caseId) noexcept { m_loadCaseId = caseId; }

    const std::string& name() const noexcept { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Forces (kN)
    double fx() const noexcept { return m_fx; }
    void setFx(double fx) noexcept { m_fx = fx; }

    double fy() const noexcept { return m_fy; }
    void setFy(double fy) noexcept { m_fy = fy; }

    double fz() const noexcept { return m_fz; }
    void setFz(double fz) noexcept { m_fz = fz; }

    void setForces(double fx, double fy, double fz) noexcept
    {
        m_fx = fx;
        m_fy = fy;
        m_fz = fz;
    }

    // Moments (kNm)
    double mx() const noexcept { return m_mx; }
    void setMx(double mx) noexcept { m_mx = mx; }

    double my() const noexcept { return m_my; }
    void setMy(double my) noexcept { m_my = my; }

    double mz() const noexcept { return m_mz; }
    void setMz(double mz) noexcept { m_mz = mz; }

    void setMoments(double mx, double my, double mz) noexcept
    {
        m_mx = mx;
        m_my = my;
        m_mz = mz;
    }

    // Système de coordonnées
    LoadCoordSystem coordSystem() const noexcept { return m_coordSys; }
    void setCoordSystem(LoadCoordSystem sys) noexcept { m_coordSys = sys; }

    // Utilitaires de diagnostic
    bool hasForce() const noexcept
    {
        return std::abs(m_fx) > 1e-9 || std::abs(m_fy) > 1e-9 || std::abs(m_fz) > 1e-9;
    }

    bool hasMoment() const noexcept
    {
        return std::abs(m_mx) > 1e-9 || std::abs(m_my) > 1e-9 || std::abs(m_mz) > 1e-9;
    }

    double forceMagnitude() const noexcept
    {
        return std::sqrt(m_fx * m_fx + m_fy * m_fy + m_fz * m_fz);
    }

    double resultantForce() const noexcept
    {
        return forceMagnitude();
    }

    double momentMagnitude() const noexcept
    {
        return std::sqrt(m_mx * m_mx + m_my * m_my + m_mz * m_mz);
    }

    double resultantMoment() const noexcept
    {
        return momentMagnitude();
    }

private:
    int m_id = 0;
    int m_nodeId = 0;
    int m_loadCaseId = 1;
    double m_fx = 0.0;
    double m_fy = 0.0;
    double m_fz = 0.0;
    double m_mx = 0.0;
    double m_my = 0.0;
    double m_mz = 0.0;
    LoadCoordSystem m_coordSys = LoadCoordSystem::Global;
    std::string m_name;
};

} // namespace TSA::Model
