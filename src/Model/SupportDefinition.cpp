#include "SupportDefinition.h"
#include <cmath>
#include <sstream>

namespace TSA::Model
{

SupportDefinition::SupportDefinition(
    DOFState tx, DOFState ty, DOFState tz,
    DOFState rx, DOFState ry, DOFState rz,
    double kx, double ky, double kz,
    double krx, double kry, double krz,
    SupportOrientationType orient,
    double dirX, double dirY, double dirZ,
    int refElemId)
    : m_tx(tx), m_ty(ty), m_tz(tz)
    , m_rx(rx), m_ry(ry), m_rz(rz)
    , m_kx(kx), m_ky(ky), m_kz(kz)
    , m_krx(krx), m_kry(kry), m_krz(krz)
    , m_orientation(orient)
    , m_customDirX(dirX), m_customDirY(dirY), m_customDirZ(dirZ)
    , m_refElemId(refElemId)
{
}

SupportDefinition SupportDefinition::free()
{
    return SupportDefinition();
}

SupportDefinition SupportDefinition::fixed()
{
    return SupportDefinition(
        DOFState::Fixed, DOFState::Fixed, DOFState::Fixed,
        DOFState::Fixed, DOFState::Fixed, DOFState::Fixed
    );
}

SupportDefinition SupportDefinition::pinned()
{
    return SupportDefinition(
        DOFState::Fixed, DOFState::Fixed, DOFState::Fixed,
        DOFState::Free, DOFState::Free, DOFState::Free
    );
}

SupportDefinition SupportDefinition::roller(double dirX, double dirY, double dirZ)
{
    SupportDefinition supp(
        DOFState::Free, DOFState::Free, DOFState::Fixed,
        DOFState::Free, DOFState::Free, DOFState::Free
    );
    supp.setCustomDirection(dirX, dirY, dirZ);
    return supp;
}

SupportDefinition SupportDefinition::sliding(double dirX, double dirY, double dirZ)
{
    SupportDefinition supp(
        DOFState::Free, DOFState::Fixed, DOFState::Fixed,
        DOFState::Fixed, DOFState::Fixed, DOFState::Fixed
    );
    supp.setCustomDirection(dirX, dirY, dirZ);
    return supp;
}

SupportDefinition SupportDefinition::linear(double axisX, double axisY, double axisZ)
{
    SupportDefinition supp(
        DOFState::Free, DOFState::Fixed, DOFState::Fixed,
        DOFState::Free, DOFState::Free, DOFState::Free
    );
    supp.setCustomDirection(axisX, axisY, axisZ);
    return supp;
}

SupportDefinition SupportDefinition::planar(double normX, double normY, double normZ)
{
    SupportDefinition supp(
        DOFState::Free, DOFState::Free, DOFState::Fixed,
        DOFState::Free, DOFState::Free, DOFState::Free
    );
    supp.setCustomDirection(normX, normY, normZ);
    return supp;
}

SupportDefinition SupportDefinition::elastic(double kx, double ky, double kz, double krx, double kry, double krz)
{
    return SupportDefinition(
        (kx > 0.0) ? DOFState::Spring : DOFState::Free,
        (ky > 0.0) ? DOFState::Spring : DOFState::Free,
        (kz > 0.0) ? DOFState::Spring : DOFState::Free,
        (krx > 0.0) ? DOFState::Spring : DOFState::Free,
        (kry > 0.0) ? DOFState::Spring : DOFState::Free,
        (krz > 0.0) ? DOFState::Spring : DOFState::Free,
        kx, ky, kz, krx, kry, krz
    );
}

SupportDefinition SupportDefinition::custom(DOFState tx, DOFState ty, DOFState tz, DOFState rx, DOFState ry, DOFState rz)
{
    return SupportDefinition(tx, ty, tz, rx, ry, rz);
}

bool SupportDefinition::isFree() const
{
    return m_tx == DOFState::Free && m_ty == DOFState::Free && m_tz == DOFState::Free &&
           m_rx == DOFState::Free && m_ry == DOFState::Free && m_rz == DOFState::Free;
}

bool SupportDefinition::isFixed() const
{
    return m_tx == DOFState::Fixed && m_ty == DOFState::Fixed && m_tz == DOFState::Fixed &&
           m_rx == DOFState::Fixed && m_ry == DOFState::Fixed && m_rz == DOFState::Fixed;
}

bool SupportDefinition::isPinned() const
{
    return m_tx == DOFState::Fixed && m_ty == DOFState::Fixed && m_tz == DOFState::Fixed &&
           m_rx == DOFState::Free && m_ry == DOFState::Free && m_rz == DOFState::Free;
}

bool SupportDefinition::isRoller() const
{
    return m_tz == DOFState::Fixed && m_tx == DOFState::Free && m_ty == DOFState::Free &&
           m_rx == DOFState::Free && m_ry == DOFState::Free && m_rz == DOFState::Free;
}

bool SupportDefinition::hasSprings() const
{
    return m_tx == DOFState::Spring || m_ty == DOFState::Spring || m_tz == DOFState::Spring ||
           m_rx == DOFState::Spring || m_ry == DOFState::Spring || m_rz == DOFState::Spring;
}

SupportType SupportDefinition::supportType() const
{
    if (isFree()) return SupportType::Free;
    if (isFixed()) return SupportType::Fixed;
    if (isPinned()) return SupportType::Pinned;
    if (isRoller()) return SupportType::Roller;
    if (hasSprings()) return SupportType::Elastic;
    return SupportType::Custom;
}

std::string SupportDefinition::typeName() const
{
    switch (supportType())
    {
    case SupportType::Free:    return "Libre";
    case SupportType::Fixed:   return "Encastrement";
    case SupportType::Pinned:  return "Articulation";
    case SupportType::Roller:  return "Appui simple";
    case SupportType::Sliding: return "Appui glissant";
    case SupportType::Linear:  return "Appui linéaire";
    case SupportType::Planar:  return "Appui plan";
    case SupportType::Elastic: return "Appui élastique";
    case SupportType::Custom:  return "Personnalisé";
    }
    return "Appui";
}

std::string SupportDefinition::dofSummary() const
{
    std::ostringstream ss;
    bool first = true;
    auto addDOF = [&](const char* name, DOFState s) {
        if (s == DOFState::Fixed)
        {
            if (!first) ss << " ";
            ss << name;
            first = false;
        }
        else if (s == DOFState::Spring)
        {
            if (!first) ss << " ";
            ss << name << "(k)";
            first = false;
        }
    };
    addDOF("UX", m_tx);
    addDOF("UY", m_ty);
    addDOF("UZ", m_tz);
    addDOF("RX", m_rx);
    addDOF("RY", m_ry);
    addDOF("RZ", m_rz);

    if (first) return "Libre";
    return ss.str();
}

SupportType SupportDefinition::toLegacySupportType() const
{
    // Retourne le type exact déterminé par supportType() pour ne pas tronquer
    // les types riches (Elastic, Custom, Sliding, Linear, Planar).
    // Les anciens consommateurs qui n'utilisent que Free/Fixed/Pinned/Roller
    // traitent les cas inconnus comme Free (pas de fixation), ce qui est plus
    // sûr que de les traiter comme Fixed (encastrement parasite).
    return supportType();
}

SupportDefinition SupportDefinition::fromLegacySupportType(SupportType type)
{
    switch (type)
    {
    case SupportType::Fixed:   return fixed();
    case SupportType::Pinned:  return pinned();
    case SupportType::Roller:  return roller();
    case SupportType::Free:
    default:
        return free();
    }
}

bool SupportDefinition::operator==(const SupportDefinition& o) const
{
    return m_tx == o.m_tx && m_ty == o.m_ty && m_tz == o.m_tz &&
           m_rx == o.m_rx && m_ry == o.m_ry && m_rz == o.m_rz &&
           std::abs(m_kx - o.m_kx) < 1e-9 &&
           std::abs(m_ky - o.m_ky) < 1e-9 &&
           std::abs(m_kz - o.m_kz) < 1e-9 &&
           std::abs(m_krx - o.m_krx) < 1e-9 &&
           std::abs(m_kry - o.m_kry) < 1e-9 &&
           std::abs(m_krz - o.m_krz) < 1e-9 &&
           m_orientation == o.m_orientation &&
           std::abs(m_customDirX - o.m_customDirX) < 1e-9 &&
           std::abs(m_customDirY - o.m_customDirY) < 1e-9 &&
           std::abs(m_customDirZ - o.m_customDirZ) < 1e-9 &&
           m_refElemId == o.m_refElemId;
}

} // namespace TSA::Model
