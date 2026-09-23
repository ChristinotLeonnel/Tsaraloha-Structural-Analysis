#pragma once

#include "Material.h"
#include <string>

namespace TSA::Model
{

class Model;

class Wall
{
public:
    Wall() = default;
    Wall(int id, int startNodeId, int endNodeId, double height = 3.0, double thickness = 0.20, const std::string& name = "");

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    std::string formattedName() const;

    int startNodeId() const { return m_startNodeId; }
    void setStartNodeId(int id) { m_startNodeId = id; }

    int endNodeId() const { return m_endNodeId; }
    void setEndNodeId(int id) { m_endNodeId = id; }

    double height() const { return m_height; }
    void setHeight(double h) { m_height = h; }

    double thickness() const { return m_thickness; }
    void setThickness(double t) { m_thickness = t; }

    const Material& material() const { return m_material; }
    Material& material() { return m_material; }
    void setMaterial(const Material& m) { m_material = m; }

    double offset() const { return m_offset; }
    void setOffset(double off) { m_offset = off; }

    double length(const Model& model) const;
    double area(const Model& model) const;

private:
    int m_id = 0;
    std::string m_name;
    int m_startNodeId = 0;
    int m_endNodeId = 0;
    double m_height = 3.0;     // Hauteur en mètres
    double m_thickness = 0.20; // Épaisseur en mètres
    Material m_material = Material::concreteC25_30();
    double m_offset = 0.0;     // Décalage par rapport à l'axe (m)
};

} // namespace TSA::Model
