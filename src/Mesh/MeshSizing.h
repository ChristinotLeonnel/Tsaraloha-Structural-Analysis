#pragma once

// Évaluation des tailles demandées par l'utilisateur (aucune génération ici) : taille de référence d'un
// objet, zones de raffinement résolues (points, nœuds, appuis, charges, contours d'ouvertures, zones
// automatiques). Utilisée pour l'estimation rapide, le nombre de nœuds des contraintes transfinies et la
// traduction des zones en champs de taille Gmsh (même définition partout).

#include "MeshGenerator.h"

namespace TSA::Mesh
{

struct ResolvedZone
{
    std::string name;
    bool segment = false;            ///< le long d'un segment (contour d'ouverture) plutôt qu'autour d'un point
    P3 a, b;
    double radius = 0.5;
    std::optional<double> size, factor;
    std::vector<std::string> targets;   ///< vide : tous les objets
};

class SizeEvaluator
{
public:
    SizeEvaluator(const MeshSettingsSet& settings, const MeshGeometry& geometry);

    double clamp(double h) const;
    double base(const std::string& ref) const;
    /// Taille au point p pour l'objet ref (zones comprises, transition linéaire hors du rayon).
    double at(const P3& p, const std::string& ref) const;
    /// Taille imposée par la zone pour l'objet ref (base × facteur ou taille), sans tenir compte de la distance.
    double zoneSize(const ResolvedZone& z, const std::string& ref) const;
    bool zoneApplies(const ResolvedZone& z, const std::string& ref) const;
    /// Une zone réduit-elle la taille sur cet objet (vérifié aux points d'échantillonnage) ?
    bool refined(const std::string& ref, const std::vector<P3>& samples) const;
    /// Nombre naturel de divisions d'un segment pour ces objets (∫ ds / h).
    int divisions(const P3& a, const P3& b, const std::vector<std::string>& refs) const;
    const std::vector<ResolvedZone>& zones() const { return m_zones; }
    const MeshSettingsSet& settings() const { return m_settings; }

private:
    const MeshSettingsSet& m_settings;
    std::vector<ResolvedZone> m_zones;
};

} // namespace TSA::Mesh
