#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <QString>
#include <gp_Pnt.hxx>

namespace TSA::Model
{
class Model;
}

namespace TSA::Analysis
{
class ResultsModel;
struct StationForces;
struct ElementResults;
}

namespace TSA::NDC
{

/**
 * @brief Données détaillées d'un extremum mécanique (sollicitation ou déplacement).
 * Permet la traçabilité complète : valeur, unité, élément, station locale, et coordonnées 3D WCS.
 */
struct ExtremumPoint
{
    QString quantityName;       ///< Ex: "Moment Fléchissant Mz", "Effort Normal N"
    double value = 0.0;         ///< Valeur algébrique
    double absValue = 0.0;      ///< Valeur absolue
    QString unit;               ///< Ex: "kNm", "kN", "mm"
    
    int elementId = 0;          ///< ID de l'élément (0 si nodal pur)
    QString elementType;        ///< "Poutre", "Poteau", "Treillis", "Câble"
    QString sectionName;        ///< Nom du profilé assigné
    QString materialName;       ///< Nom du matériau
    
    int nodeId = 0;             ///< ID du nœud (si extremum nodal ou extrémité)
    double localPositionX = 0.0;///< Position x le long de la barre (m)
    double relativePosition = 0.0; ///< Ratio x / L (0.0 à 1.0)
    double memberLength = 0.0;  ///< Longueur totale de la barre (m)
    
    gp_Pnt globalCoords;        ///< Coordonnées 3D globales WCS (X, Y, Z)
    QString loadCaseOrCombo;    ///< Cas de charge ou combinaison gouvernante
    
    // Pour la flèche : ratio de portée L / f
    double spanToDeflectionRatio = 0.0; ///< Ex: 345 pour L/345
    bool exceedsStandardLimit = false;  ///< Vrai si f > L/250 (ou norme ELS)
};

/**
 * @brief Synthèse de l'élément le plus sollicité pour un critère donné.
 */
struct MostStressedSummary
{
    ExtremumPoint maxBendingMz;     ///< Moment fléchissant Mz maximal
    ExtremumPoint minBendingMz;     ///< Moment fléchissant Mz minimal (en travée ou sur appui)
    ExtremumPoint absMaxBendingMz;  ///< Moment fléchissant Mz max absolu
    
    ExtremumPoint maxBendingMy;     ///< Moment fléchissant My maximal
    ExtremumPoint absMaxBendingMy;  ///< Moment My max absolu
    
    ExtremumPoint maxShearVz;       ///< Effort tranchant Vz maximal
    ExtremumPoint absMaxShearVz;    ///< Effort tranchant Vz max absolu
    ExtremumPoint absMaxShearVy;    ///< Effort tranchant Vy max absolu
    
    ExtremumPoint maxTensionN;      ///< Traction axiale N maximale (N > 0)
    ExtremumPoint maxCompressionN;  ///< Compression axiale N maximale (N < 0, valeur absolue max)
    
    ExtremumPoint maxTorsionMx;     ///< Torsion Mx maximale
    
    ExtremumPoint maxDeflection;    ///< Flèche transversale maximale le long des poutres
    ExtremumPoint maxDisplacement;  ///< Déplacement nodal résultant maximal
    ExtremumPoint maxReaction;      ///< Réaction d'appui résultante maximale
};

/**
 * @brief Analyseur et extracteur d'extrema mécaniques pour le module de reporting.
 * [NORM: ISO/IEC 25010, EN 1990 §6, IEEE Std 1063]
 */
class ResultAnalyzer
{
public:
    ResultAnalyzer() = default;

    /**
     * @brief Analyse l'ensemble des résultats du solveur et extrait les points critiques.
     */
    static MostStressedSummary analyzeExtrema(
        const TSA::Model::Model& model,
        const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
        const QString& governingCombination = QStringLiteral("ELU Fondamentale")
    );

    /**
     * @brief Calcule les coordonnées 3D globales d'un point à l'abscisse locale x d'une barre.
     */
    static gp_Pnt computeGlobalPoint(
        const TSA::Model::Model& model,
        int elementId,
        double localX
    );
};

} // namespace TSA::NDC
