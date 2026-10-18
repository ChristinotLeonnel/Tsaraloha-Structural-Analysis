#pragma once

#include <string>
#include <QtGlobal>

namespace TSA::Model
{

/**
 * @brief Types de charges supportées dans TSA.
 */
enum class LoadType
{
    NodalForce,         ///< Force ponctuelle nodale (Fx, Fy, Fz)
    NodalMoment,        ///< Moment concentré nodal (Mx, My, Mz)
    MemberUniform,      ///< Charge linéique uniforme sur barre (kN/m)
    MemberLinear,       ///< Charge linéique variable trapézoïdale / triangulaire (kN/m)
    MemberPoint,        ///< Force ponctuelle appliquée le long d'une barre (kN)
    MemberMoment,       ///< Moment réparti sur barre (kNm/m)
    SelfWeight          ///< Poids propre gravitaire calculé automatiquement
};

/**
 * @brief Type d'élément structural porteur de la charge sur barre.
 */
enum class MemberTargetType
{
    Beam,               ///< Poutre / barre générale (Model::beams)
    Column,             ///< Poteau (Model::columns)
    Truss,              ///< Barre de treillis (Model::trussMembers)
    Cable               ///< Câble (Model::cables)
};

/**
 * @brief Système de coordonnées de définition de la charge.
 */
enum class LoadCoordSystem
{
    Global,             ///< Repère global WCS (X, Y, Z)
    Local               ///< Repère local LCS de l'élément (x longitudinal, y, z inertie)
};

/**
 * @brief Direction d'application de la charge.
 */
enum class LoadDirection
{
    GlobalX,            ///< Axe global +X
    GlobalY,            ///< Axe global +Y
    GlobalZ,            ///< Axe global +Z
    Gravity,            ///< Gravité globale (-Z par défaut dans TSA)
    LocalX,             ///< Axe local x (longitudinal)
    LocalY,             ///< Axe local y (axe fort/faible de la section)
    LocalZ              ///< Axe local z (axe orthogonal de la section)
};

/**
 * @brief Catégorie normalisée de cas de charge (Eurocode EN 1990 / EN 1991).
 */
enum class LoadCaseCategory
{
    Dead,               ///< G : Charges permanentes et poids propre
    Live,               ///< Q : Charges d'exploitation
    Wind,               ///< W : Actions du vent
    Snow,               ///< S : Charges de neige
    Seismic,            ///< E : Actions sismiques (séisme)
    Temperature,        ///< T : Variations de température
    Accidental,         ///< A : Actions accidentelles (chocs, incendie)
    Custom              ///< Cas de charge personnalisé
};

/**
 * @brief Type de combinaison d'actions structurales.
 */
enum class LoadCombinationType
{
    ULS_Fundamental,    ///< ELU Fondamental (ex: 1.35 G + 1.50 Q)
    ULS_Accidental,     ///< ELU Accidentel
    ULS_Seismic,        ///< ELU Sismique
    SLS_Characteristic, ///< ELS Caractéristique (ex: 1.00 G + 1.00 Q)
    SLS_Frequent,       ///< ELS Fréquente
    SLS_QuasiPermanent, ///< ELS Quasi-permanente
    Custom,             ///< Combinaison personnalisée

    // Alias usuels
    ULS = ULS_Fundamental,
    SLS = SLS_Characteristic
};

/**
 * @brief Utilitaires de conversion en texte lisible pour l'UI et les rapports.
 */
inline std::string loadCategoryToString(LoadCaseCategory cat)
{
    switch (cat)
    {
    case LoadCaseCategory::Dead:        return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "G (Permanent)");
    case LoadCaseCategory::Live:        return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Q (Exploitation)");
    case LoadCaseCategory::Wind:        return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "W (Vent)");
    case LoadCaseCategory::Snow:        return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "S (Neige)");
    case LoadCaseCategory::Seismic:     return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "E (Séisme)");
    case LoadCaseCategory::Temperature: return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "T (Thermique)");
    case LoadCaseCategory::Accidental:  return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "A (Accidentel)");
    case LoadCaseCategory::Custom:      return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Personnalisé");
    }
    return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Inconnu");
}

inline std::string loadDirectionToString(LoadDirection dir)
{
    switch (dir)
    {
    case LoadDirection::GlobalX: return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Global X");
    case LoadDirection::GlobalY: return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Global Y");
    case LoadDirection::GlobalZ: return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Global Z");
    case LoadDirection::Gravity: return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Gravité (-Z)");
    case LoadDirection::LocalX:  return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Local x");
    case LoadDirection::LocalY:  return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Local y");
    case LoadDirection::LocalZ:  return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Local z");
    }
    return QT_TRANSLATE_NOOP("TSA::Model::LoadEnums", "Global Z");
}

} // namespace TSA::Model
