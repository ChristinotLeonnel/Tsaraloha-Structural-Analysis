#include "Slab.h"
#include "Model.h"
#include "Node.h"
#include <cmath>

namespace TSA::Model
{

Slab::Slab(int id, const std::vector<int>& nodeIds, double thickness)
    : m_id(id)
    , m_nodeIds(nodeIds)
    , m_thickness(thickness)
{
}

double Slab::area(const Model& model) const
{
    if (m_nodeIds.size() < 3)
        return 0.0;

    // Calcul de l'aire d'un polygone 3D via somme des produits vectoriels
    double totalVx = 0.0;
    double totalVy = 0.0;
    double totalVz = 0.0;

    const auto* origin = model.getNode(m_nodeIds[0]);
    if (!origin)
        return 0.0;

    for (size_t i = 1; i + 1 < m_nodeIds.size(); ++i)
    {
        const auto* p1 = model.getNode(m_nodeIds[i]);
        const auto* p2 = model.getNode(m_nodeIds[i + 1]);
        if (!p1 || !p2)
            continue;

        double v1x = p1->x() - origin->x();
        double v1y = p1->y() - origin->y();
        double v1z = p1->z() - origin->z();

        double v2x = p2->x() - origin->x();
        double v2y = p2->y() - origin->y();
        double v2z = p2->z() - origin->z();

        // Produit vectoriel v1 x v2
        double cx = v1y * v2z - v1z * v2y;
        double cy = v1z * v2x - v1x * v2z;
        double cz = v1x * v2y - v1y * v2x;

        totalVx += cx;
        totalVy += cy;
        totalVz += cz;
    }

    return 0.5 * std::sqrt(totalVx * totalVx + totalVy * totalVy + totalVz * totalVz);
}

} // namespace TSA::Model
