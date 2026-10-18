#pragma once

// Formulation mécanique souhaitée d'un élément surfacique (dalle, voile), indépendante du moteur de calcul.
// Chaque moteur déclare les formulations qu'il sait réellement calculer (adaptateur) ; une formulation non prise
// en charge est refusée avant le calcul, jamais remplacée silencieusement par une autre.

#include <string>
#include <QtGlobal>

namespace TSA::Model
{

enum class SurfaceFormulation
{
    Shell,          ///< coque : membrane (efforts dans le plan) + plaque (flexion, torsion, cisaillement transverse)
    Plate,          ///< plaque seule : flexion hors plan, sans rigidité de membrane
    Membrane,       ///< membrane seule : efforts dans le plan, sans rigidité de flexion
    NonStructural   ///< géométrie seulement (rendu, métrés) : non transmise au calcul
};

inline const char* surfaceFormulationKey(SurfaceFormulation f)
{
    switch (f)
    {
    case SurfaceFormulation::Plate: return "plate";
    case SurfaceFormulation::Membrane: return "membrane";
    case SurfaceFormulation::NonStructural: return "none";
    default: return "shell";
    }
}

inline SurfaceFormulation surfaceFormulationFromKey(const std::string& k)
{
    if (k == "plate") return SurfaceFormulation::Plate;
    if (k == "membrane") return SurfaceFormulation::Membrane;
    if (k == "none") return SurfaceFormulation::NonStructural;
    return SurfaceFormulation::Shell;
}

inline const char* surfaceFormulationName(SurfaceFormulation f)
{
    switch (f)
    {
    case SurfaceFormulation::Plate: return QT_TRANSLATE_NOOP("TSA::Model::SurfaceFormulation", "Plaque (flexion seule)");
    case SurfaceFormulation::Membrane: return QT_TRANSLATE_NOOP("TSA::Model::SurfaceFormulation", "Membrane (efforts dans le plan)");
    case SurfaceFormulation::NonStructural: return QT_TRANSLATE_NOOP("TSA::Model::SurfaceFormulation", "Non structurale (géométrie seulement)");
    default: return QT_TRANSLATE_NOOP("TSA::Model::SurfaceFormulation", "Coque (membrane + flexion)");
    }
}

} // namespace TSA::Model
