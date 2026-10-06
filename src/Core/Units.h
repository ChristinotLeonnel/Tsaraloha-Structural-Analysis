#pragma once

// Unités centralisées de TSA.
// Unités internes de référence du modèle (SI) : longueur m, force N, masse kg, contrainte Pa,
// moment N·m, masse volumique kg/m³, angle rad. Les moteurs peuvent travailler dans un autre
// système (kN, kPa) : la conversion passe par ces fonctions, jamais par des facteurs écrits à la main.

#include <string>
#include <string_view>

namespace TSA::Units
{

enum class Quantity
{
    Length,
    Area,
    SecondMomentOfArea,
    Force,
    Moment,
    Stress,
    Mass,
    Density,
    LinearForce,   ///< force par unité de longueur
    Angle
};

/// Facteur vers l'unité SI de référence de la grandeur (ex. "mm" → 1e-3 pour Length).
/// Retourne 0 si l'unité n'est pas reconnue pour cette grandeur.
constexpr double toSI(Quantity q, std::string_view unit)
{
    switch (q)
    {
    case Quantity::Length:
        return unit == "m" ? 1.0 : unit == "cm" ? 1e-2 : unit == "mm" ? 1e-3 : unit == "km" ? 1e3 : 0.0;
    case Quantity::Area:
        return unit == "m2" ? 1.0 : unit == "cm2" ? 1e-4 : unit == "mm2" ? 1e-6 : 0.0;
    case Quantity::SecondMomentOfArea:
        return unit == "m4" ? 1.0 : unit == "cm4" ? 1e-8 : unit == "mm4" ? 1e-12 : 0.0;
    case Quantity::Force:
        return unit == "N" ? 1.0 : unit == "kN" ? 1e3 : unit == "MN" ? 1e6 : 0.0;
    case Quantity::Moment:
        return unit == "N.m" ? 1.0 : unit == "kN.m" ? 1e3 : unit == "MN.m" ? 1e6 : 0.0;
    case Quantity::Stress:
        return unit == "Pa" ? 1.0 : unit == "kPa" ? 1e3 : unit == "MPa" ? 1e6 : unit == "GPa" ? 1e9 : 0.0;
    case Quantity::Mass:
        return unit == "kg" ? 1.0 : unit == "t" ? 1e3 : unit == "g" ? 1e-3 : 0.0;
    case Quantity::Density:
        return unit == "kg/m3" ? 1.0 : unit == "t/m3" ? 1e3 : 0.0;
    case Quantity::LinearForce:
        return unit == "N/m" ? 1.0 : unit == "kN/m" ? 1e3 : 0.0;
    case Quantity::Angle:
        return unit == "rad" ? 1.0 : unit == "deg" ? 3.14159265358979323846 / 180.0 : 0.0;
    }
    return 0.0;
}

/// Convertit une valeur entre deux unités d'une même grandeur.
constexpr double convert(Quantity q, double value, std::string_view from, std::string_view to)
{
    const double a = toSI(q, from), b = toSI(q, to);
    return (a == 0.0 || b == 0.0) ? 0.0 : value * a / b;
}

// Raccourcis des conversions les plus fréquentes du code de calcul
constexpr double paToKPa(double pa) { return convert(Quantity::Stress, pa, "Pa", "kPa"); }
constexpr double paToMPa(double pa) { return convert(Quantity::Stress, pa, "Pa", "MPa"); }
constexpr double mToMm(double m) { return convert(Quantity::Length, m, "m", "mm"); }
constexpr double mmToM(double mm) { return convert(Quantity::Length, mm, "mm", "m"); }
constexpr double degToRad(double d) { return convert(Quantity::Angle, d, "deg", "rad"); }
constexpr double radToDeg(double r) { return convert(Quantity::Angle, r, "rad", "deg"); }

/// Accélération de la pesanteur utilisée pour le poids propre (m/s²), valeur unique de TSA.
inline constexpr double kGravity = 9.81;

} // namespace TSA::Units
