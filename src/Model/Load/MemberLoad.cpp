#include "MemberLoad.h"

#include <algorithm>

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

std::pair<double, double> MemberLoad::appliedRange(double length) const
{
    const double L = std::max(0.0, length);
    auto absolute = [&](double x) { return std::clamp(m_isRelative ? x * L : x, 0.0, L); };
    const double a = absolute(m_x1);
    switch (m_type)
    {
    case LoadType::MemberPoint:
        return { a, a };
    case LoadType::MemberLinear:
        return { a, m_x2 > m_x1 ? absolute(m_x2) : L };
    default:
        if (m_x2 > m_x1) return { a, absolute(m_x2) };
        return { 0.0, L };
    }
}

bool MemberLoad::coversFullLength(double length) const
{
    const auto [a, b] = appliedRange(length);
    const double tol = 1e-9 * std::max(1.0, length);
    return a <= tol && b >= length - tol;
}

double MemberLoad::intensityAt(double s, double length) const
{
    if (m_type != LoadType::MemberLinear) return m_q1;
    const auto [a, b] = appliedRange(length);
    if (b - a <= 1e-12) return m_q1;
    const double t = std::clamp((s - a) / (b - a), 0.0, 1.0);
    return m_q1 + (m_q2 - m_q1) * t;
}

std::string MemberLoad::validate(double length) const
{
    if (!std::isfinite(m_q1) || !std::isfinite(m_q2) || !std::isfinite(m_x1) || !std::isfinite(m_x2))
        return "Valeurs non finies (intensité ou position).";
    if (!(length > 1e-9))
        return "La barre chargée est de longueur nulle.";
    const bool linear = m_type == LoadType::MemberLinear;
    if (std::abs(m_q1) < 1e-12 && (!linear || std::abs(m_q2) < 1e-12))
        return "L'intensité de la charge est nulle.";

    const double maxPos = m_isRelative ? 1.0 : length;
    const double tol = 1e-9 * std::max(1.0, maxPos);
    auto inside = [&](double x) { return x >= -tol && x <= maxPos + tol; };
    if (!inside(m_x1)) return "La position de début est hors de la barre.";
    if (m_type == LoadType::MemberPoint) return {};
    if (!inside(m_x2)) return "La position de fin est hors de la barre.";
    // x2 = 0 reste la lecture historique « jusqu'au nœud j » ; une fin explicite doit suivre le début.
    if (m_x2 > tol && m_x2 <= m_x1)
        return "La position de fin doit être supérieure à la position de début.";
    const auto [a, b] = appliedRange(length);
    if (b - a <= 1e-6)
        return "L'intervalle chargé est vide.";
    return {};
}

} // namespace TSA::Model
