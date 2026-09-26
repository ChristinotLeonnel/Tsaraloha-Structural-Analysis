#pragma once

#include "Material.h"
#include <vector>
#include <string>

namespace TSA::Model
{

class Model;

enum class SlabType
{
    TwoWay,     // Portance bidirectionnelle
    OneWay,     // Portance unidirectionnelle
    FlatSlab    // Dalle champignon / plancher-dalle
};

class Slab
{
public:
    Slab() = default;
    Slab(int id, const std::vector<int>& nodeIds, double thickness = 0.20, const std::string& name = "", SlabType type = SlabType::TwoWay);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    std::string formattedName() const;

    const std::vector<int>& nodeIds() const { return m_nodeIds; }
    void setNodeIds(const std::vector<int>& nodeIds) { m_nodeIds = nodeIds; }

    double thickness() const { return m_thickness; }
    void setThickness(double thickness) { m_thickness = thickness; }

    const Material& material() const { return m_material; }
    Material& material() { return m_material; }
    void setMaterial(const Material& material) { m_material = material; }
    int materialId() const { return m_material.id; }
    void setMaterialId(int id);

    SlabType slabType() const { return m_slabType; }
    void setSlabType(SlabType type) { m_slabType = type; }

    double area(const Model& model) const;

    const std::string& color() const { return m_color; }
    void setColor(const std::string& color) { m_color = color; }

private:
    int m_id = 0;
    std::string m_name;
    std::vector<int> m_nodeIds;
    double m_thickness = 0.20; // Épaisseur en mètres
    Material m_material = Material::concreteC25_30();
    SlabType m_slabType = SlabType::TwoWay;
    std::string m_color;
};

} // namespace TSA::Model
