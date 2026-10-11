#pragma once

// Répartition d'une charge surfacique sur les éléments 2D d'une surface maillée (docs/MESH.md §K) :
// charges nodales équivalentes COHÉRENTES (∫ Nᵢ p dA, intégration de Gauss sur les fonctions de forme des
// éléments tri3, tri6, quad4, quad8), direction globale (X, Y, Z, gravité) ou normale locale, surface
// réelle ou projetée. La résultante et le moment par rapport à l'origine sont contrôlés contre la valeur
// analytique (aire nette de la surface, ouvertures déduites, et son centre) ; un écart, une surface non
// maillée ou des éléments non pris en charge sont signalés : la charge n'est alors pas convertie.
//
// Gmsh n'est ici que le générateur du maillage : la charge est un objet TSA, la répartition un service TSA.

#include "MeshGeometry.h"
#include "MeshTypes.h"

#include <string>
#include <vector>

namespace TSA::Mesh
{

struct SurfacePressure
{
    std::string surface;            ///< référence de la surface (SL2, W5)
    double value = 0;               ///< kN/m²
    enum class Direction
    {
        GlobalX,
        GlobalY,
        GlobalZ,
        Gravity,        ///< −Z global
        LocalNormal     ///< −N (normale de la surface, sens de sa définition) : une pression positive appuie sur la face
    } direction = Direction::Gravity;
    bool projected = false;         ///< direction globale : intensité rapportée à la surface projetée perpendiculairement à la direction
    /// Variation linéaire dans le repère local de la surface : p(u, v) = value + gradU · u + gradV · v (kN/m³) ; ex. pression
    /// hydrostatique sur un voile : value = γ·h, gradV = −γ, zone limitée à v ≤ h.
    double gradU = 0, gradV = 0;
    /// Zone chargée : rectangle [zoneU0, zoneU1] × [zoneV0, zoneV1] du repère local (sinon toute la surface nette).
    bool zone = false;
    double zoneU0 = 0, zoneV0 = 0, zoneU1 = 0, zoneV1 = 0;
};

struct NodalForce
{
    int node = 0;                   ///< nœud du maillage
    double fx = 0, fy = 0, fz = 0;  ///< kN
};

struct SurfaceLoadResult
{
    bool ok = false;
    std::vector<NodalForce> forces;
    std::size_t elements = 0;
    P3 resultant, expectedResultant;          ///< kN
    P3 moment, expectedMoment;                ///< kN·m, par rapport à l'origine
    double resultantError = 0, momentError = 0;   ///< écarts relatifs
    std::vector<std::string> errors, warnings;
};

struct SurfacePointLoad
{
    std::string surface;            ///< SL2, W5
    double u = 0, v = 0;            ///< position dans le repère local de la surface (m)
    double value = 0;               ///< kN
    SurfacePressure::Direction direction = SurfacePressure::Direction::Gravity;
};

/// Force ponctuelle sur une surface maillée : répartie sur les nœuds de l'élément qui contient le point par ses fonctions
/// de forme (Nᵢ(ξ, η) · F, statiquement équivalent). Refusée si le point est hors de la surface ou dans une ouverture.
SurfaceLoadResult distributeSurfacePointLoad(const FeMesh& mesh, const MeshGeometry& geometry, const SurfacePointLoad& load, double tolerance = 1e-6);

SurfaceLoadResult distributeSurfaceLoad(const FeMesh& mesh, const MeshGeometry& geometry, const SurfacePressure& load, double tolerance = 1e-6);

} // namespace TSA::Mesh
