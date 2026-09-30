#pragma once

#include <QString>

namespace TSA::UI
{

enum class PortType
{
    Model3D,            ///< Modèle géométrique 3D (CAD / Modélisation)
    Results3D,          ///< Rendu 3D des résultats (Déformée, Diagrammes 3D, Réactions)
    Diagram2D,          ///< Courbes et diagrammes 2D interactifs (M, V, N, Pushover)
    CalculationNote     ///< Note de calcul interactive (NDC)
};

enum class PortLayout
{
    Single,             ///< 1 seul port plein écran
    SplitHorizontal,    ///< 2 ports côte à côte (gauche / droite)
    SplitVertical,      ///< 2 ports superposés (haut / bas)
    Grid2x2,            ///< 4 ports en grille 2x2
    Tabbed              ///< Ports sous forme d'onglets
};

inline QString portTypeName(PortType type)
{
    switch (type)
    {
    case PortType::Model3D:         return QStringLiteral("Vue 3D Modèle");
    case PortType::Results3D:       return QStringLiteral("Vue 3D Résultats");
    case PortType::Diagram2D:       return QStringLiteral("Diagrammes 2D");
    case PortType::CalculationNote: return QStringLiteral("Note de Calcul");
    default:                        return QStringLiteral("Vue");
    }
}

} // namespace TSA::UI
