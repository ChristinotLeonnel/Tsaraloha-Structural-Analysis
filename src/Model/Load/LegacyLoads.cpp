#include "LegacyLoads.h"

#include <cmath>

namespace TSA::Model
{

MemberLoad memberLoadFromStored(int id, int elementId, int loadCaseId, LoadType type, double q1, double q2, std::uint8_t direction,
                                LoadCoordSystem coordSystem, double x1, double x2, bool isRelative, const std::string& name,
                                MemberTargetType targetType)
{
    const bool storedLocal = direction >= static_cast<std::uint8_t>(LoadDirection::LocalX);
    const bool axisMatches = direction != kLegacyGravityDirection && storedLocal == (coordSystem == LoadCoordSystem::Local);
    if (axisMatches)
        return MemberLoad(id, elementId, loadCaseId, type, q1, q2, static_cast<LoadDirection>(direction), x1, x2, isRelative, name, targetType);
    // Direction sans axe : seule l'intensité comptait, vers le bas du repère stocké
    const LoadDirection axis = coordSystem == LoadCoordSystem::Local ? LoadDirection::LocalZ : LoadDirection::GlobalZ;
    return MemberLoad(id, elementId, loadCaseId, type, -std::abs(q1), -std::abs(q2), axis, x1, x2, isRelative, name, targetType);
}

void applyStoredSurfaceDirection(SurfaceLoad& load, const std::string& key)
{
    if (key == "x") load.direction = SurfaceLoadDirection::GlobalX;
    else if (key == "y") load.direction = SurfaceLoadDirection::GlobalY;
    else if (key == "z") load.direction = SurfaceLoadDirection::GlobalZ;
    else if (key == "localX") load.direction = SurfaceLoadDirection::LocalX;
    else if (key == "localY") load.direction = SurfaceLoadDirection::LocalY;
    else if (key == "localZ") load.direction = SurfaceLoadDirection::LocalZ;
    else
    {
        load.direction = key == "normal" ? SurfaceLoadDirection::LocalZ : SurfaceLoadDirection::GlobalZ;
        load.value = -load.value;
        load.gradU = -load.gradU;
        load.gradV = -load.gradV;
    }
}

} // namespace TSA::Model
