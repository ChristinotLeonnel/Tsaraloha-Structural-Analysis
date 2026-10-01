#pragma once

#include <string>
#include <vector>
#include <map>
#include <QString>

namespace TSA::Standards
{

/**
 * @brief Cadres normatifs internationaux et européens de référence pour TSA.
 * [NORM: ISO/IEC/IEEE 12207, ISO/IEC 25010, EN 1990 à EN 1999, IEEE Std 1063]
 */
enum class StandardFramework
{
    // Normes de génie logiciel et qualité
    ISO_IEC_25010,          ///< Qualité du produit logiciel (SQuaRE)
    ISO_IEC_IEEE_12207,     ///< Processus du cycle de vie logiciel
    ISO_IEC_IEEE_29119,     ///< Stratégie, conception et exécution des tests logiciels
    ISO_IEC_IEEE_29148,     ///< Ingénierie des exigences
    ISO_9001,               ///< Système de gestion de la qualité et traçabilité
    ISO_IEC_27001,          ///< Sécurité et intégrité des données
    IEEE_Std_1063,          ///< Norme de documentation utilisateur et technique

    // Eurocodes de calcul structural
    EN_1990,                ///< Eurocode 0 : Bases de calcul des structures
    EN_1991,                ///< Eurocode 1 : Actions sur les structures
    EN_1992,                ///< Eurocode 2 : Calcul des structures en béton
    EN_1993,                ///< Eurocode 3 : Calcul des structures en acier
    EN_1994,                ///< Eurocode 4 : Calcul des structures mixtes acier-béton
    EN_1995,                ///< Eurocode 5 : Calcul des structures en bois
    EN_1996,                ///< Eurocode 6 : Calcul des structures en maçonnerie
    EN_1997,                ///< Eurocode 7 : Calcul géotechnique
    EN_1998,                ///< Eurocode 8 : Calcul des structures pour leur résistance aux séismes
    EN_1999,                ///< Eurocode 9 : Calcul des structures en aluminium
    EN_1993_1_11,           ///< Eurocode 3 Partie 1-11 : Éléments tendus et câbles

    // Normes de produits et matériaux
    EN_10138,               ///< Aciers de précontrainte (torons, fils, barres)
    ASTM_A416,              ///< Standard Specification for Steel Strand
    fib_Bulletin_89         ///< Recommandations internationales fib pour haubans
};

/**
 * @brief Statut formel de mise en œuvre d'une exigence normative.
 */
enum class RequirementStatus
{
    Implemented,            ///< Exigence implémentée, validée et couverte par des tests
    Partial,                ///< Implémentation partielle ou en cours
    Missing,                ///< Exigence identifiée mais pas encore implémentée
    NotApplicable,          ///< Exigence non applicable dans le périmètre actuel
    ToVerify                ///< Statut ou référence normative nécessitant vérification
};

/**
 * @brief Domaine fonctionnel d'application de l'exigence.
 */
enum class RequirementDomain
{
    Architecture,           ///< Architecture logicielle, découplage, modularité
    DataModel,              ///< Modèle de données métier, unités SI, persistance
    AnalysisFEM,            ///< Moteur de calcul éléments finis, solveur, convergence
    StructuralDesign,       ///< Vérification réglementaire selon Eurocodes
    ResultsVisualization,   ///< Traitement et affichage 3D des résultats
    TestingQA,              ///< Stratégie de test et non-régression
    DocumentationUI,        ///< Documentation, ergonomie, catalogue de commandes
    SecurityIntegrity       ///< Intégrité mémoire, gestion des pannes, validation
};

/**
 * @brief Fiche d'exigence normative traçable.
 * Relie une norme à son code d'implémentation et à ses tests unitaires.
 */
struct NormativeRequirement
{
    std::string id;                 ///< Identifiant canonique (ex: "REQ-SW-ARCH-001", "REQ-CALC-EC0-001")
    StandardFramework standard = StandardFramework::ISO_IEC_25010;
    RequirementDomain domain = RequirementDomain::Architecture;
    std::string standardTitle;      ///< Titre formel de la norme (ex: "EN 1990:2002+A1:2005")
    std::string clause;             ///< Clause ou paragraphe exact vérifié (ex: "§6.4.3")
    std::string requirementText;    ///< Énoncé précis et vérifiable de l'exigence
    std::string codeLocation;       ///< Fichier(s) source ou classe(s) concerné(s)
    std::string associatedTest;      ///< Test unitaire / intégration validant l'exigence
    RequirementStatus status = RequirementStatus::ToVerify;
    std::string validationNotes;    ///< Remarques d'audit et conditions de conformité
};

// Fonctions utilitaires de conversion
QString standardFrameworkToString(StandardFramework stdCode);
QString requirementStatusToString(RequirementStatus status);
QString requirementDomainToString(RequirementDomain domain);

} // namespace TSA::Standards
