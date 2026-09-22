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

bool Column::isVertical(const Model& model, double tol) const
{
    const auto* n1 = model.getNode(m_startNodeId);
    const auto* n2 = model.getNode(m_endNodeId);
    if (!n1 || !n2)
        return false;

    double dx = std::abs(n2->x() - n1->x());
    double dy = std::abs(n2->y() - n1->y());
    double dz = std::abs(n2->z() - n1->z());

    return (dx <= tol && dy <= tol && dz > tol);
}

double Column::bottomElevation(const Model& model) const
{
    const auto* n1 = model.getNode(m_startNodeId);
    const auto* n2 = model.getNode(m_endNodeId);
    if (!n1 || !n2)
        return 0.0;
    return std::min(n1->z(), n2->z());
}

double Column::topElevation(const Model& model) const
{
    const auto* n1 = model.getNode(m_startNodeId);
    const auto* n2 = model.getNode(m_endNodeId);
    if (!n1 || !n2)
        return 0.0;
    return std::max(n1->z(), n2->z());
}

std::string Column::direction(const Model& model) const
{
    return isVertical(model) ? "Vertical Z" : "Incliné";
}

} // namespace TSA::Model
