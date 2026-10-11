#pragma once

// Charge appliquée à un élément surfacique (dalle ou voile) : pression uniforme (kN/m²) ou charge ponctuelle (kN)
// en un point du plan de la surface. La conversion en forces nodales est faite par le maillage (charges nodales
// cohérentes ∫ Nᵢ p dA, docs/SURFACES.md) ; la charge reste un objet TSA, indépendant du moteur de calcul.

#include <string>

namespace TSA::Model
{

enum class SurfaceTargetKind
{
    Slab,
    Wall
};

enum class SurfaceLoadDirection
{
    Gravity,        ///< −Z global (valeur positive vers le bas)
    GlobalX,
    GlobalY,
    GlobalZ,
    LocalNormal     ///< −N (normale locale de la surface) : une valeur positive appuie sur la face supérieure
};

enum class SurfaceLoadDistribution
{
    Uniform,        ///< pression sur toute la surface nette (ouvertures déduites), kN/m²
    Point           ///< force concentrée au point (u, v) du repère local de la surface, kN
};

struct SurfaceLoad
{
    int id = 0;
    SurfaceTargetKind targetKind = SurfaceTargetKind::Slab;
    int surfaceId = 0;
    int loadCaseId = 0;
    std::string name;
    SurfaceLoadDistribution distribution = SurfaceLoadDistribution::Uniform;
    SurfaceLoadDirection direction = SurfaceLoadDirection::Gravity;
    double value = 0.0;         ///< kN/m² (uniforme) ou kN (ponctuelle)
    bool projected = false;     ///< uniforme, direction globale : intensité rapportée à la surface projetée
    double u = 0.0, v = 0.0;    ///< ponctuelle : position dans le repère local de la surface (m)
    /// Uniforme : variation linéaire p(u, v) = value + gradU · u + gradV · v (kN/m³, repère local ; ex. hydrostatique)
    double gradU = 0.0, gradV = 0.0;
    /// Uniforme : zone chargée limitée au rectangle [zoneU0, zoneU1] × [zoneV0, zoneV1] du repère local (m)
    bool zone = false;
    double zoneU0 = 0.0, zoneV0 = 0.0, zoneU1 = 0.0, zoneV1 = 0.0;

    /// Référence de la surface, identique à celle du maillage (SL2, W5).
    std::string surfaceRef() const { return (targetKind == SurfaceTargetKind::Slab ? "SL" : "W") + std::to_string(surfaceId); }
    bool operator==(const SurfaceLoad&) const = default;
};

inline const char* surfaceLoadDirectionKey(SurfaceLoadDirection d)
{
    switch (d)
    {
    case SurfaceLoadDirection::GlobalX: return "x";
    case SurfaceLoadDirection::GlobalY: return "y";
    case SurfaceLoadDirection::GlobalZ: return "z";
    case SurfaceLoadDirection::LocalNormal: return "normal";
    default: return "gravity";
    }
}

inline SurfaceLoadDirection surfaceLoadDirectionFromKey(const std::string& k)
{
    if (k == "x") return SurfaceLoadDirection::GlobalX;
    if (k == "y") return SurfaceLoadDirection::GlobalY;
    if (k == "z") return SurfaceLoadDirection::GlobalZ;
    if (k == "normal") return SurfaceLoadDirection::LocalNormal;
    return SurfaceLoadDirection::Gravity;
}

} // namespace TSA::Model
