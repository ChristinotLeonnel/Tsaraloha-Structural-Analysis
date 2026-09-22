#pragma once

#include <vector>

namespace TSA::Model
{

class Model;

class Slab
{
public:
    Slab() = default;
    Slab(int id, const std::vector<int>& nodeIds, double thickness = 0.20);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::vector<int>& nodeIds() const { return m_nodeIds; }
    void setNodeIds(const std::vector<int>& nodeIds) { m_nodeIds = nodeIds; }

    double thickness() const { return m_thickness; }
    void setThickness(double thickness) { m_thickness = thickness; }

    double area(const Model& model) const;

private:
    int m_id = 0;
    std::vector<int> m_nodeIds;
    double m_thickness = 0.20; // Épaisseur en mètres (ex: 20 cm)
};

} // namespace TSA::Model
