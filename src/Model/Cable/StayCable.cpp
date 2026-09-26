#include "StayCable.h"
#include "../Model.h"
#include "../Node.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TSA::Model
{

StayCable::StayCable()
    : Cable(0, 0, 0, "StayCable_1", CableType::StayCable)
{
    setColor("#D9531E"); // Teinte orange vif spécifique aux haubans
    setDefinition(CableDefinition::stayCablePSS_19x15_7());
}

StayCable::StayCable(int id, int towerNodeId, int deckNodeId, const std::string& name, const CableDefinition& definition)
    : Cable(id, towerNodeId, deckNodeId, definition, name.empty() ? ("StayCable_" + std::to_string(id)) : name)
{
    setType(CableType::StayCable);
    setColor("#D9531E");
}

StayCable::StayCable(int id, int towerNodeId, int deckNodeId, const CableDefinition& definition, const std::string& name)
    : Cable(id, towerNodeId, deckNodeId, definition, name.empty() ? ("StayCable_" + std::to_string(id)) : name)
{
    setType(CableType::StayCable);
    setColor("#D9531E");
}

double StayCable::inclinationAngleDeg(const Model& model) const
{
    const auto* nTower = model.getNode(towerNodeId());
    const auto* nDeck = model.getNode(deckNodeId());
    if (!nTower || !nDeck)
        return 0.0;

    double dx = nDeck->x() - nTower->x();
    double dy = nDeck->y() - nTower->y();
    double dz = std::abs(nTower->z() - nDeck->z());
    double Lh = std::sqrt(dx * dx + dy * dy);

    if (Lh < 1e-6)
    {
        return 90.0;
    }

    double rad = std::atan2(dz, Lh);
    return rad * (180.0 / M_PI);
}

} // namespace TSA::Model
