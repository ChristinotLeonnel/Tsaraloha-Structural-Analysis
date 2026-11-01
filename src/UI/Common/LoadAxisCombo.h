#pragma once

// Choix du repère et de l'axe d'une charge, le même dans toutes les fenêtres : X, Y, Z globaux ou x, y, z locaux, aux couleurs des
// axes de la vue 3D (X / x rouge, Y / y vert, Z / z bleu).

#include <QComboBox>

#include <array>

namespace TSA::UI
{

/// Remplit combo avec les trois axes du repère ; values : donnée de chaque axe (X, Y, Z). L'axe courant est conservé par position.
void fillLoadAxisCombo(QComboBox* combo, bool local, const std::array<int, 3>& values);

} // namespace TSA::UI
