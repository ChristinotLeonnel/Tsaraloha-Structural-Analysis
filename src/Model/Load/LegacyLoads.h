#pragma once

// Lecture des charges d'anciens fichiers (.tsa, .tsa3d, cas de validation). Une charge n'a plus que des axes : X, Y, Z globaux ou
// x, y, z locaux. Les directions retirées sont converties à la lecture, sans changer l'action physique : l'ancienne direction
// « gravité » (toujours vers −Z, le signe saisi était ignoré) devient une charge suivant Z global d'intensité −|q|.

#include "MemberLoad.h"
#include "SurfaceLoad.h"

#include <cstdint>
#include <string>

namespace TSA::Model
{

/// Valeur de l'ancienne direction « gravité » dans les fichiers ; LoadDirection ne l'utilise plus.
constexpr std::uint8_t kLegacyGravityDirection = 3;

/// Charge sur barre lue d'un fichier. direction : valeur stockée. Un axe incohérent avec le repère stocké (axe local en repère
/// global, axe global en repère local) était lu par les moteurs comme une gravité (−Z global) ou une charge selon −z local.
MemberLoad memberLoadFromStored(int id, int elementId, int loadCaseId, LoadType type, double q1, double q2, std::uint8_t direction,
                                LoadCoordSystem coordSystem, double x1, double x2, bool isRelative, const std::string& name,
                                MemberTargetType targetType);

/// Direction d'une charge surfacique lue d'un fichier, value et gradients déjà renseignés. « gravity » (ou clé absente) et
/// « normal » (−N) deviennent Z global et z local, de valeurs opposées.
void applyStoredSurfaceDirection(SurfaceLoad& load, const std::string& key);

} // namespace TSA::Model
