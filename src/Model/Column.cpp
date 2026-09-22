#include "Column.h"
#include "Model.h"
#include "Node.h"
#include <cmath>

namespace TSA::Model
{

Column::Column(int id, int startNodeId, int endNodeId, double width, double height)
    : m_id(id)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_width(width)
    , m_height(height)
{
}

void Column::setDimensions(double width, double height)
{
    m_width = width;
    m_height = height;
}

double Column::length(const Model& model) const
{
    const auto* n1 = model.getNode(m_startNodeId);
    const auto* n2 = model.getNode(m_endNodeId);
    if (!n1 || !n2)
        return 0.0;

    double dx = n2->x() - n1->x();
    double dy = n2->y() - n1->y();
    double dz = n2->z() - n1->z();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace TSA::Model
