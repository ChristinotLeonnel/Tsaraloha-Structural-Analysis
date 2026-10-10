#pragma once

// Représentation graphique d'une charge sur barre, indépendante de l'affichage (aucun objet AIS).
//
// Chaîne : MemberLoad (modèle, source de vérité) → MemberLoad::appliedRange / intensityAt (forme)
//          → LoadResolver::memberLoadVector (sens physique, même convention que les moteurs)
//          → MemberLoadGlyph (flèches, enveloppe) → OccView (primitives OCCT).
//
// Les valeurs physiques ne sont jamais modifiées : seule la longueur des flèches est mise à l'échelle,
// proportionnellement à l'intensité locale (|q| / |q|max), le sens restant celui du vecteur physique.

#include "../Model/Load/MemberLoad.h"

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <vector>

namespace TSA::Geometry
{

struct MemberLoadGlyphSettings
{
    double arrowLength = 0.6;   ///< longueur (m) de la flèche d'intensité maximale
    double spacing = 0.27;      ///< espacement visé entre deux flèches (m)
    int minArrows = 3;          ///< pour un intervalle au moins égal à l'espacement
    int maxArrows = 60;         ///< plafond par charge (barres très longues, grands modèles)
};

/// Flèche dont la pointe touche la barre ; elle s'étend du côté d'où vient la charge.
struct LoadArrow
{
    double position = 0.0;      ///< abscisse sur la barre (m depuis le nœud i)
    double intensity = 0.0;     ///< intensité locale signée (kN/m ou kN)
    gp_Pnt tip;                 ///< point d'application, sur la barre
    gp_Pnt tail;                ///< origine de la flèche (tip − direction × length)
    gp_Dir direction;           ///< sens physique de la charge
    double length = 0.0;        ///< longueur affichée (m)
};

enum class MemberLoadGlyphKind
{
    Invalid,            ///< barre introuvable ou de longueur nulle
    Distributed,        ///< uniforme ou linéaire (triangulaire, trapézoïdale)
    Point,              ///< force ponctuelle sur la barre
    DistributedMoment   ///< moment réparti : pas de flèche de force (non transmis aux moteurs)
};

struct MemberLoadGlyph
{
    MemberLoadGlyphKind kind = MemberLoadGlyphKind::Invalid;
    double start = 0.0;                 ///< début de l'intervalle chargé (m)
    double end = 0.0;                   ///< fin de l'intervalle chargé (m)
    std::vector<LoadArrow> arrows;      ///< flèches d'intensité non nulle
    std::vector<gp_Pnt> envelope;       ///< queues des flèches de a à b (sur la barre où q = 0)
    double maxIntensity = 0.0;          ///< |q| maximal sur l'intervalle (kN/m ou kN)
    gp_Pnt labelAnchor;                 ///< position proposée pour l'étiquette
};

/// Nombre de stations régulièrement espacées pour un intervalle de longueur span :
/// round(span / spacing) + 1 borné à [minArrows, maxArrows] ; 2 pour un intervalle plus court que
/// l'espacement (pas de superposition), 1 pour un intervalle quasi nul.
int distributedStationCount(double span, const MemberLoadGlyphSettings& settings);

/// Représentation de la charge sur la barre p1 → p2 (rotation beta en degrés).
MemberLoadGlyph buildMemberLoadGlyph(const TSA::Model::MemberLoad& load,
                                     const gp_Pnt& p1, const gp_Pnt& p2, double betaAngleDeg,
                                     const MemberLoadGlyphSettings& settings = {});

} // namespace TSA::Geometry
