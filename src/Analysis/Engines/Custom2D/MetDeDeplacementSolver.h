#pragma once

// Solveur Custom2D branché sur la bibliothèque MetDeDeplacement (thirdparty/MetDeDeplacement) :
// méthode des déplacements pour ossatures planes. Ce pont combine les cas de charge selon la
// demande (mêmes règles que le générateur OpenSees), ajoute le poids propre, puis convertit
// les résultats au contrat Custom2D (indices, conventions de signe).

#include "Custom2DSolver.h"

namespace TSA::Analysis::Custom2D
{

class MetDeDeplacementSolver final : public ISolver
{
public:
    std::string name() const override;
    std::string version() const override;
    Output solve(const Input& input) override;
    Features features() const override { return { true, true, true }; }
};

} // namespace TSA::Analysis::Custom2D
