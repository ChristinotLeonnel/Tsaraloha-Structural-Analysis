#pragma once

#include <string>

namespace TSA::Model
{

/**
 * @brief Typologie des câbles et éléments tendus en génie civil et travaux publics.
 * Conforme aux classifications Eurocode 3 (EN 1993-1-11), Eurocode 2 (EN 1992-1-1) et ASTM.
 */
enum class CableType
{
    Generic,                ///< Câble générique de traction
    Strand,                 ///< Toron élémentaire (ex: toron 7 fils EN 10138-3 / ASTM A416)
    Wire,                   ///< Fil tréfilé haute résistance (EN 10138-2)
    PrestressingBar,        ///< Barre de précontrainte filetée (ex: Macalloy / SAS / Freyssinet)
    StayCable,              ///< Hauban de pont / structure haubanée (faisceau de torons gainés / torons clos)
    SuspensionCable,        ///< Câble porteur principal de pont suspendu (caténaire principale)
    Hanger,                 ///< Suspente verticale ou inclinée de pont suspendu
    ExternalPrestressing,   ///< Câble de précontrainte extérieure (au béton ou métallique)
    GroundAnchor            ///< Tirant d'ancrage actif ou passif scellé au terrain
};

/**
 * @brief Mode de définition géométrique du profil longitudinal du câble.
 */
enum class CableGeometryMode
{
    Straight,       ///< Câble droit rectiligne tendu entre deux nœuds
    Polyline,       ///< Ligne brisée passant par des déviateurs ou selles intermédiaires
    Parabolic,      ///< Profil parabolique défini par flèche (sag) ou équation y = 4*f*(x/L)*(1-x/L)
    Catenary,       ///< Véritable caténaire élastique / chaînette d'équilibre y = a*(cosh(x/a) - 1)
    Spline,         ///< Courbe B-Spline lisse passant par un ensemble de points de guidage
    ThroughPoints   ///< Passage obligé par une série discrète de nœuds ou selles intermédiaires
};

/**
 * @brief Type d'ancrage aux extrémités du câble.
 */
enum class AnchorType
{
    Fixed,                  ///< Ancrage passif fixe / bloqué (butée fixe, culot coulé)
    Pinned,                 ///< Ancrage articulé (chape, axe, œil)
    PrestressingAnchor,     ///< Ancrage actif de précontrainte réglable (trompette, mors, vérinage)
    GroundAnchor,           ///< Tête d'ancrage au rocher / terrain avec scellement
    StructuralAnchor        ///< Ancrage solidaire d'un nœud structurel (pylône, tablier)
};

/**
 * @brief Normes et référentiels officiels de dimensionnement des câbles.
 */
enum class StandardCode
{
    EN_1993_1_11,   ///< Eurocode 3 Partie 1-11 : Structures avec éléments tendus
    EN_10138_3,     ///< Norme européenne : Aciers de précontrainte - Torons
    EN_10138_4,     ///< Norme européenne : Aciers de précontrainte - Barres
    EN_1992_1_1,    ///< Eurocode 2 : Calcul des structures en béton (Précontrainte)
    ASTM_A416,      ///< Standard Specification for Low-Relaxation Seven-Wire Steel Strand
    ASTM_A586,      ///< Zinc-Coated Structural Strand
    AASHTO_LRFD,    ///< AASHTO LRFD Bridge Design Specifications (Cables & Tendons)
    CustomStandard  ///< Référentiel personnalisé ou spécification fabricant
};

inline std::string cableTypeToString(CableType type)
{
    switch (type)
    {
    case CableType::Generic: return "Generic";
    case CableType::Strand: return "Strand";
    case CableType::Wire: return "Wire";
    case CableType::PrestressingBar: return "PrestressingBar";
    case CableType::StayCable: return "StayCable";
    case CableType::SuspensionCable: return "SuspensionCable";
    case CableType::Hanger: return "Hanger";
    case CableType::ExternalPrestressing: return "ExternalPrestressing";
    case CableType::GroundAnchor: return "GroundAnchor";
    default: return "Generic";
    }
}

inline CableType stringToCableType(const std::string& str)
{
    if (str == "Strand") return CableType::Strand;
    if (str == "Wire") return CableType::Wire;
    if (str == "PrestressingBar") return CableType::PrestressingBar;
    if (str == "StayCable") return CableType::StayCable;
    if (str == "SuspensionCable") return CableType::SuspensionCable;
    if (str == "Hanger") return CableType::Hanger;
    if (str == "ExternalPrestressing") return CableType::ExternalPrestressing;
    if (str == "GroundAnchor") return CableType::GroundAnchor;
    return CableType::Generic;
}

inline std::string geometryModeToString(CableGeometryMode mode)
{
    switch (mode)
    {
    case CableGeometryMode::Straight: return "Straight";
    case CableGeometryMode::Polyline: return "Polyline";
    case CableGeometryMode::Parabolic: return "Parabolic";
    case CableGeometryMode::Catenary: return "Catenary";
    case CableGeometryMode::Spline: return "Spline";
    case CableGeometryMode::ThroughPoints: return "ThroughPoints";
    default: return "Straight";
    }
}

inline CableGeometryMode stringToGeometryMode(const std::string& str)
{
    if (str == "Polyline") return CableGeometryMode::Polyline;
    if (str == "Parabolic") return CableGeometryMode::Parabolic;
    if (str == "Catenary") return CableGeometryMode::Catenary;
    if (str == "Spline") return CableGeometryMode::Spline;
    if (str == "ThroughPoints") return CableGeometryMode::ThroughPoints;
    return CableGeometryMode::Straight;
}

inline std::string anchorTypeToString(AnchorType type)
{
    switch (type)
    {
    case AnchorType::Fixed: return "Fixed";
    case AnchorType::Pinned: return "Pinned";
    case AnchorType::PrestressingAnchor: return "PrestressingAnchor";
    case AnchorType::GroundAnchor: return "GroundAnchor";
    case AnchorType::StructuralAnchor: return "StructuralAnchor";
    default: return "Fixed";
    }
}

inline AnchorType stringToAnchorType(const std::string& str)
{
    if (str == "Pinned") return AnchorType::Pinned;
    if (str == "PrestressingAnchor") return AnchorType::PrestressingAnchor;
    if (str == "GroundAnchor") return AnchorType::GroundAnchor;
    if (str == "StructuralAnchor") return AnchorType::StructuralAnchor;
    return AnchorType::Fixed;
}

inline std::string standardCodeToString(StandardCode code)
{
    switch (code)
    {
    case StandardCode::EN_1993_1_11: return "EN 1993-1-11";
    case StandardCode::EN_10138_3: return "EN 10138-3";
    case StandardCode::EN_10138_4: return "EN 10138-4";
    case StandardCode::EN_1992_1_1: return "EN 1992-1-1";
    case StandardCode::ASTM_A416: return "ASTM A416";
    case StandardCode::ASTM_A586: return "ASTM A586";
    case StandardCode::AASHTO_LRFD: return "AASHTO LRFD";
    case StandardCode::CustomStandard: return "Custom";
    default: return "Custom";
    }
}

inline StandardCode stringToStandardCode(const std::string& str)
{
    if (str == "EN 1993-1-11") return StandardCode::EN_1993_1_11;
    if (str == "EN 10138-3") return StandardCode::EN_10138_3;
    if (str == "EN 10138-4") return StandardCode::EN_10138_4;
    if (str == "EN 1992-1-1") return StandardCode::EN_1992_1_1;
    if (str == "ASTM A416") return StandardCode::ASTM_A416;
    if (str == "ASTM A586") return StandardCode::ASTM_A586;
    if (str == "AASHTO LRFD") return StandardCode::AASHTO_LRFD;
    return StandardCode::CustomStandard;
}

} // namespace TSA::Model
