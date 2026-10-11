#include "Slab.h"
#include <algorithm>
#include "Model.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Model
{

Slab::Slab(int id, const std::vector<int>& nodeIds, double thickness, const std::string& name, SlabType type)
    : m_id(id)
    , m_name(name)
    , m_nodeIds(nodeIds)
    , m_thickness(thickness)
    , m_material(Material::concreteC25_30())
    , m_slabType(type)
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        ss << "S" << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

std::string Slab::formattedName() const
{
    if (!m_name.empty())
        return m_name;
    std::ostringstream ss;
    ss << "S" << std::setw(3) << std::setfill('0') << m_id;
    return ss.str();
}

double Slab::grossArea(const Model& model) const
{
    if (m_nodeIds.size() < 3) return 0.0;

    // Calcul d'aire 3D via la formule du vecteur aire (somme des produits vectoriels)
    double ax = 0.0, ay = 0.0, az = 0.0;
    size_t n = m_nodeIds.size();

    for (size_t i = 0; i < n; ++i)
    {
        const auto* p1 = model.getNode(m_nodeIds[i]);
        const auto* p2 = model.getNode(m_nodeIds[(i + 1) % n]);
        if (!p1 || !p2) return 0.0;

        ax += (p1->y() * p2->z() - p1->z() * p2->y());
        ay += (p1->z() * p2->x() - p1->x() * p2->z());
        az += (p1->x() * p2->y() - p1->y() * p2->x());
    }

    return 0.5 * std::sqrt(ax * ax + ay * ay + az * az);
}

void Slab::setMaterialId(int id)
{
    m_material = Material::findById(id);
}

double polygonArea(const std::vector<std::pair<double, double>>& poly)
{
    double a = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i)
    {
        const auto& [x1, y1] = poly[i];
        const auto& [x2, y2] = poly[(i + 1) % poly.size()];
        a += x1 * y2 - x2 * y1;
    }
    return 0.5 * a;
}

double Slab::area(const Model& model) const
{
    double a = grossArea(model);
    for (const auto& o : m_openings) a -= std::abs(polygonArea(o.polygon));
    return std::max(0.0, a);
}

} // namespace TSA::Model
