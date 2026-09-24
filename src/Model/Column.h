#pragma once

#include "Section.h"
#include "Material.h"
#include <string>

namespace TSA::Model
{

class Model;

class Column
{
public:
    Column() = default;
    Column(int id, int startNodeId, int endNodeId, double width = 0.30, double height = 0.30, const std::string& name = "");

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    std::string formattedName() const;

    int startNodeId() const { return m_startNodeId; }
    void setStartNodeId(int nodeId) { m_startNodeId = nodeId; }

    int endNodeId() const { return m_endNodeId; }
    void setEndNodeId(int nodeId) { m_endNodeId = nodeId; }

    double width() const { return m_section.width; }
    void setWidth(double width) { m_section.width = width; }

    double height() const { return m_section.height; }
    void setHeight(double height) { m_section.height = height; }

    double depth() const { return m_section.height; }
    void setDepth(double depth) { m_section.height = depth; }

    void setDimensions(double width, double height);

    const Section& section() const { return m_section; }
    Section& section() { return m_section; }
    void setSection(const Section& section) { m_section = section; }

    const Material& material() const { return m_material; }
    Material& material() { return m_material; }
    void setMaterial(const Material& material) { m_material = material; }

    double rotation() const { return m_rotation; }
    void setRotation(double degrees) { m_rotation = degrees; }

    double length(const Model& model) const;
    bool isVertical(const Model& model, double tol = 1e-3) const;
    double bottomElevation(const Model& model) const;
    double topElevation(const Model& model) const;
    std::string direction(const Model& model) const;

    const std::string& color() const { return m_color; }
    void setColor(const std::string& color) { m_color = color; }

private:
    int m_id = 0;
    std::string m_name;
    int m_startNodeId = 0;
    int m_endNodeId = 0;
    Section m_section = Section::rectangular(0.30, 0.30);
    Material m_material = Material::concreteC25_30();
    double m_rotation = 0.0;
    std::string m_color;
};

} // namespace TSA::Model
