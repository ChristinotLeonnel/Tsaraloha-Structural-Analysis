#pragma once

#include "LoadEnums.h"
#include <string>
#include <cmath>

namespace TSA::Model
{

/**
 * @brief Représente une charge appliquée à un élément structural linéaire (poutre, poteau, barre).
 * Prend en charge les charges uniformes, linéiques variables (trapézoïdales/triangulaires),
 * ponctuelles concentrées le long de la barre et moments répartis.
 */
class MemberLoad
{
public:
    MemberLoad() = default;
    MemberLoad(int id, int elementId, int loadCaseId,
               LoadType type = LoadType::MemberUniform,
               double q1 = 0.0, double q2 = 0.0,
               LoadDirection direction = LoadDirection::Gravity,
               LoadCoordSystem coordSys = LoadCoordSystem::Global,
               double x1 = 0.0, double x2 = 0.0,
               bool isRelative = false,
               const std::string& name = "",
               MemberTargetType targetType = MemberTargetType::Beam);

    // Usines pratiques (Static Factories)
    static MemberLoad uniform(int id, int elementId, int loadCaseId,
                              double q, LoadDirection dir = LoadDirection::Gravity,
                              LoadCoordSystem sys = LoadCoordSystem::Global,
                              const std::string& name = "",
                              MemberTargetType targetType = MemberTargetType::Beam);

    static MemberLoad uniform(int elementId, int loadCaseId,
                              double q, LoadDirection dir = LoadDirection::Gravity,
                              LoadCoordSystem sys = LoadCoordSystem::Global,
                              const std::string& name = "",
                              MemberTargetType targetType = MemberTargetType::Beam)
    {
        return uniform(0, elementId, loadCaseId, q, dir, sys, name, targetType);
    }

    static MemberLoad uniform(int elementId, int loadCaseId,
                              double q, LoadDirection dir,
                              const std::string& name,
                              MemberTargetType targetType = MemberTargetType::Beam)
    {
        return uniform(0, elementId, loadCaseId, q, dir, LoadCoordSystem::Global, name, targetType);
    }

    static MemberLoad trapezoidal(int id, int elementId, int loadCaseId,
                                 double q1, double q2, double x1, double x2,
                                 LoadDirection dir = LoadDirection::Gravity,
                                 LoadCoordSystem sys = LoadCoordSystem::Global,
                                 bool isRelative = false,
                                 const std::string& name = "");

    static MemberLoad trapezoidal(int elementId, int loadCaseId,
                                  double q1, double q2, double x1, double x2,
                                  LoadDirection dir = LoadDirection::Gravity,
                                  LoadCoordSystem sys = LoadCoordSystem::Global,
                                  bool isRelative = false,
                                  const std::string& name = "")
    {
        return trapezoidal(0, elementId, loadCaseId, q1, q2, x1, x2, dir, sys, isRelative, name);
    }

    static MemberLoad trapezoidal(int elementId, int loadCaseId,
                                  double q1, double q2, double x1, double x2,
                                  LoadDirection dir,
                                  const std::string& name)
    {
        return trapezoidal(0, elementId, loadCaseId, q1, q2, x1, x2, dir, LoadCoordSystem::Global, false, name);
    }

    static MemberLoad pointOnMember(int id, int elementId, int loadCaseId,
                                    double p, double position,
                                    LoadDirection dir = LoadDirection::Gravity,
                                    LoadCoordSystem sys = LoadCoordSystem::Global,
                                    bool isRelative = false,
                                    const std::string& name = "");

    static MemberLoad pointOnMember(int elementId, int loadCaseId,
                                    double p, double position,
                                    LoadDirection dir = LoadDirection::Gravity,
                                    LoadCoordSystem sys = LoadCoordSystem::Global,
                                    bool isRelative = false,
                                    const std::string& name = "")
    {
        return pointOnMember(0, elementId, loadCaseId, p, position, dir, sys, isRelative, name);
    }

    static MemberLoad pointOnMember(int elementId, int loadCaseId,
                                    double p, double position,
                                    LoadDirection dir,
                                    const std::string& name)
    {
        return pointOnMember(0, elementId, loadCaseId, p, position, dir, LoadCoordSystem::Global, false, name);
    }

    // Identifiants
    int id() const noexcept { return m_id; }
    void setId(int id) noexcept { m_id = id; }

    int elementId() const noexcept { return m_elementId; }
    void setElementId(int elemId) noexcept { m_elementId = elemId; }

    int loadCaseId() const noexcept { return m_loadCaseId; }
    void setLoadCaseId(int caseId) noexcept { m_loadCaseId = caseId; }

    const std::string& name() const noexcept { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Type & Direction
    LoadType type() const noexcept { return m_type; }
    void setType(LoadType type) noexcept { m_type = type; }

    LoadDirection direction() const noexcept { return m_direction; }
    void setDirection(LoadDirection dir) noexcept { m_direction = dir; }

    LoadCoordSystem coordSystem() const noexcept { return m_coordSys; }
    void setCoordSystem(LoadCoordSystem sys) noexcept { m_coordSys = sys; }

    // Intensités (kN/m pour répartie, kN pour ponctuelle, kNm/m pour moment réparti)
    double q1() const noexcept { return m_q1; }
    void setQ1(double q1) noexcept { m_q1 = q1; }

    double q2() const noexcept { return m_q2; }
    void setQ2(double q2) noexcept { m_q2 = q2; }

    void setUniformIntensity(double q) noexcept
    {
        m_q1 = q;
        m_q2 = q;
    }

    // Positions d'application le long de la barre (mètres ou ratio 0..1 si isRelative)
    double x1() const noexcept { return m_x1; }
    void setX1(double x1) noexcept { m_x1 = x1; }

    double x2() const noexcept { return m_x2; }
    void setX2(double x2) noexcept { m_x2 = x2; }

    bool isRelativePosition() const noexcept { return m_isRelative; }
    void setRelativePosition(bool rel) noexcept { m_isRelative = rel; }

    // Diagnostic
    bool isUniform() const noexcept
    {
        return m_type == LoadType::MemberUniform || (std::abs(m_q1 - m_q2) < 1e-9);
    }

    bool isTrapezoidal() const noexcept
    {
        return m_type == LoadType::MemberLinear;
    }

    bool isPointOnMember() const noexcept
    {
        return m_type == LoadType::MemberPoint;
    }

    bool isFullSpan() const noexcept
    {
        return std::abs(m_x1) < 1e-9 && std::abs(m_x2) < 1e-9;
    }

    MemberTargetType targetType() const noexcept { return m_targetType; }
    void setTargetType(MemberTargetType t) noexcept { m_targetType = t; }

private:
    int m_id = 0;
    int m_elementId = 0;
    int m_loadCaseId = 1;
    LoadType m_type = LoadType::MemberUniform;
    double m_q1 = 0.0;
    double m_q2 = 0.0;
    LoadDirection m_direction = LoadDirection::Gravity;
    LoadCoordSystem m_coordSys = LoadCoordSystem::Global;
    double m_x1 = 0.0;
    double m_x2 = 0.0;
    bool m_isRelative = false;
    std::string m_name;
    MemberTargetType m_targetType = MemberTargetType::Beam;
};

} // namespace TSA::Model
