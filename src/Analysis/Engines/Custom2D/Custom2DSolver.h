#pragma once

// Contrat du solveur d'ossatures planes du moteur « custom2d » : c'est l'API du cœur scientifique
// TSALab (tsalab::planar, TSALab/science/include/tsalab/planar/PlanarSolver.h), sans Qt ni modèle TSA.
// TSA n'en garde que l'adaptation de son modèle (Custom2DAdapter) et le moteur (Custom2DEngine).
// Architecture : docs/TSARALOHA_ARCHITECTURE.md (ADR-024).

#include "tsalab/planar/PlanarSolver.h"

// Espace de noms de l'intégration TSA : il expose les types du contrat TSALab (Custom2D::Input…) et
// accueille les fonctions propres à TSA (Custom2DAdapter : buildInput, mapResults).
namespace TSA::Analysis::Custom2D
{
using namespace ::tsalab::planar;
}
