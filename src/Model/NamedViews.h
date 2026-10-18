#pragma once

// Vues nommées du projet : caméra (œil, cible, haut, échelle, projection), mode 2D, face du plan 2D et plan de travail.
// Données du projet (comme les cotations), enregistrées dans le .tsa (chunk VIEW, format ≥ 1.12) : un projet ne voit
// que ses propres vues ; un ancien fichier n'en a aucune, et une version antérieure de TSA ignore le chunk.
// Données pures : l'application à la caméra est faite par la vue (OccView).

#include <array>
#include <string>
#include <vector>

namespace TSA::Model
{

struct NamedView
{
    std::string name;
    std::array<double, 3> eye { 0.0, 0.0, 1.0 };
    std::array<double, 3> center { 0.0, 0.0, 0.0 };
    std::array<double, 3> up { 0.0, 1.0, 0.0 };
    double scale = 1.0;                ///< hauteur de la vue en mètres (Graphic3d_Camera::Scale)
    bool orthographic = true;
    bool mode2D = false;
    std::string planFace = "custom";   ///< face du plan 2D : top, bottom, front, back, left, right, custom
    std::string workPlaneJson;         ///< plan de travail actif (WorkPlane::toJson), vide : inchangé

    bool operator==(const NamedView& o) const = default;
};

struct NamedViewSet
{
    std::vector<NamedView> views;

    bool operator==(const NamedViewSet& o) const = default;

    int indexOf(const std::string& name) const;
    /// Ajoute ou remplace (même nom) ; retourne l'indice.
    int upsert(const NamedView& view);
    bool rename(const std::string& from, const std::string& to);
    bool remove(const std::string& name);

    /// JSON UTF-8 versionné (« schema »). Lecture tolérante : vue invalide ignorée et signalée.
    std::string toJson() const;
    static NamedViewSet fromJson(const std::string& json, std::string* warnings = nullptr);
};

} // namespace TSA::Model
