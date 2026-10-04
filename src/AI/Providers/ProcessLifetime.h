#pragma once

#include <QtGlobal>

namespace TSA::AI
{

/// Rattache un processus enfant à la durée de vie de TSA (Windows : Job Object
/// KILL_ON_JOB_CLOSE). Si TSA se ferme ou plante, le moteur IA est arrêté par le système
/// et ne reste pas en mémoire. Renvoie false si non pris en charge.
bool bindToParentLifetime(qint64 pid);

} // namespace TSA::AI
