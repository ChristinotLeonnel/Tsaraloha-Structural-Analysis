#pragma once

namespace TSA::Coordinate
{

/// Tolérances géométriques partagées (mètres). Une seule source pour que la détection des plans,
/// l'isolation 2D et la sélection « sur le plan » donnent toujours le même résultat.
struct GeometryTolerance
{
    /// Appartenance d'un point à un plan de travail / niveau (regroupement des plans X, Y, Z).
    static constexpr double planeMembership = 0.05;
    /// Coïncidence de deux points (nœuds confondus, longueurs nulles).
    static constexpr double pointCoincidence = 1e-6;
    /// Point tracé confondu avec un nœud existant (réutilisé au lieu d'un doublon). Au-delà : nouveau
    /// nœud, jamais de fusion par proximité ; l'accrochage (SnapEngine) reste le moyen de viser un nœud.
    static constexpr double nodeCoincidence = 1e-3;
};

} // namespace TSA::Coordinate
