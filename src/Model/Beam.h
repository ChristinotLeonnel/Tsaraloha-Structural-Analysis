#pragma once

#include "Section.h"
#include "Material.h"
#include <string>

namespace TSA::Model
{

class Model;

enum class BarRole
{
    Generic,
    Beam,
    Column,
    Brace,
    Tie,
    Truss,
    SteelMember
};

enum class BarEccentricity
{
    None,
    TopFlange,
    BottomFlange,
    LeftFlange,
    RightFlange
};

struct EndRelease
{
    bool fx = false, fy = false, fz = false;
    bool mx = false, my = false, mz = false;
};

struct BarProperties
{
    int id = 1;
    std::string name = "Barre_1";
    BarRole role = BarRole::Beam;
    Section section = Section::ipe(100);
    Material material = Material::steelS235();
    double rotation = 0.0; // γ en degrés
    BarEccentricity eccentricity = BarEccentricity::None;
    EndRelease startRelease;
    EndRelease endRelease;
    std::string color;
};

class Beam
{
public:
    Beam() = default;
    Beam(int id, int startNodeId, int endNodeId, double width = 0.30, double height = 0.50, const std::string& name = "", BarRole role = BarRole::Beam);
    Beam(int id, int startNodeId, int endNodeId, const Section& section, const Material& material, BarRole role = BarRole::Beam, double rotation = 0.0, const std::string& name = "");

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    std::string formattedName() const;

    int startNodeId() const { return m_startNodeId; }
    void setStartNodeId(int nodeId) { m_startNodeId = nodeId; }

    int endNodeId() const { return m_endNodeId; }
    void setEndNodeId(int nodeId) { m_endNodeId = nodeId; }

    BarRole role() const { return m_role; }
    void setRole(BarRole r) { m_role = r; }

    double width() const { return m_section.width; }
    void setWidth(double width) { m_section.width = width; }

    double height() const { return m_section.height; }
    void setHeight(double height) { m_section.height = height; }

    void setDimensions(double width, double height);

    const Section& section() const { return m_section; }
    Section& section() { return m_section; }
    void setSection(const Section& section) { m_section = section; }

    const Material& material() const { return m_material; }
    Material& material() { return m_material; }
    void setMaterial(const Material& material) { m_material = material; }
    int materialId() const { return m_material.id; }
    void setMaterialId(int id);

    double rotation() const { return m_rotation; }
    void setRotation(double degrees) { m_rotation = degrees; }

    BarEccentricity eccentricity() const { return m_eccentricity; }
    void setEccentricity(BarEccentricity ecc) { m_eccentricity = ecc; }

    const EndRelease& startRelease() const { return m_startRelease; }
    void setStartRelease(const EndRelease& rel) { m_startRelease = rel; }

    const EndRelease& endRelease() const { return m_endRelease; }
    void setEndRelease(const EndRelease& rel) { m_endRelease = rel; }

    double length(const Model& model) const;

    const std::string& color() const { return m_color; }
    void setColor(const std::string& color) { m_color = color; }

    BarProperties properties() const;
    void setProperties(const BarProperties& props);

private:
    int m_id = 0;
    std::string m_name;
    int m_startNodeId = 0;
    int m_endNodeId = 0;
    BarRole m_role = BarRole::Beam;
    Section m_section = Section::rectangular(0.30, 0.50);
    Material m_material = Material::concreteC25_30();
    double m_rotation = 0.0; // Angle bêta / gamma en degrés
    BarEccentricity m_eccentricity = BarEccentricity::None;
    EndRelease m_startRelease;
    EndRelease m_endRelease;
    std::string m_color;
};

using Bar = Beam;

} // namespace TSA::Model
