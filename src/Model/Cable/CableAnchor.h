#pragma once

#include "CableTypes.h"
#include "../../Coordinate/Point3D.h"
#include <string>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>

namespace TSA::Model
{

/**
 * @brief Ancrage d'extrémité pour câble ou système de tension (tête d'ancrage, culot, mors, butée).
 */
class CableAnchor
{
public:
    CableAnchor();
    CableAnchor(int id, const std::string& name, AnchorType type, const TSA::Coordinate::Point3D& pos);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    AnchorType type() const { return m_type; }
    void setType(AnchorType type) { m_type = type; }

    int connectedNodeId() const { return m_connectedNodeId; }
    void setConnectedNodeId(int nodeId) { m_connectedNodeId = nodeId; }

    const TSA::Coordinate::Point3D& position() const { return m_position; }
    void setPosition(const TSA::Coordinate::Point3D& pos) { m_position = pos; }
    void setPosition(double x, double y, double z) { m_position = TSA::Coordinate::Point3D(x, y, z); }
    gp_Pnt toGpPnt() const { return m_position.toGpPnt(); }

    // Orientation de l'axe de l'ancrage
    double dirX() const { return m_dirX; }
    double dirY() const { return m_dirY; }
    double dirZ() const { return m_dirZ; }
    void setDirection(double dx, double dy, double dz);
    gp_Dir toGpDir() const;

    // Capacité résistante ultime de l'ancrage en Newtons
    double capacity() const { return m_capacity; }
    void setCapacity(double cap) { m_capacity = cap; }

    // Rentrée de mors / glissement à l'ancrage en mètres (anchorage slip)
    double slip() const { return m_slip; }
    void setSlip(double slipMeters) { m_slip = slipMeters; }

    // Diamètre extérieur du corps de l'ancrage en mètres (pour rendu 3D)
    double socketDiameter() const { return m_socketDiameter; }
    void setSocketDiameter(double d) { m_socketDiameter = d; }

    // Longueur de la douille / trompette d'ancrage en mètres
    double socketLength() const { return m_socketLength; }
    void setSocketLength(double l) { m_socketLength = l; }

    const std::string& color() const { return m_color; }
    void setColor(const std::string& col) { m_color = col; }

private:
    int m_id = 0;
    std::string m_name;
    AnchorType m_type = AnchorType::Fixed;
    int m_connectedNodeId = -1;
    TSA::Coordinate::Point3D m_position{0.0, 0.0, 0.0};
    double m_dirX = 0.0;
    double m_dirY = 0.0;
    double m_dirZ = 1.0;
    double m_capacity = 1.0e6;      // 1 MN par défaut
    double m_slip = 0.006;          // 6 mm de rentrée de mors standard (EN 1992-1-1)
    double m_socketDiameter = 0.12; // 120 mm
    double m_socketLength = 0.25;   // 250 mm
    std::string m_color = "#3A4D6B";
};

} // namespace TSA::Model
