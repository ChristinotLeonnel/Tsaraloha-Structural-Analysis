#pragma once

#include <QString>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::NDC
{

/**
 * @brief Entrée bibliographique ou webographique formelle et certifiée.
 * Respecte scrupuleusement la norme ISO 690 / IEEE Std 1063 :
 * Aucun référence fictive, sources universitaires et éditeurs officiels vérifiés.
 */
struct BibliographicReference
{
    QString citationKey;    ///< Ex: "[EC0]", "[EC2]", "[OpenSees-Recorders]"
    QString authors;        ///< Auteurs ou organisme officiel
    QString title;          ///< Titre complet du document ou standard
    QString publisher;      ///< Éditeur ou organisme de normalisation (CEN, JRC, PEER, etc.)
    int year = 0;           ///< Année de publication
    QString edition;        ///< Édition ou révision
    QString url;            ///< URL officielle vérifiée
    QString accessedDate;   ///< Date de consultation
    QString context;        ///< Rôle dans le projet TSA (ex: "Formulation des éléments finis et recorders")
};

/**
 * @brief Norme réglementaire identifiée dans le calcul.
 */
struct StandardReference
{
    QString code;           ///< Ex: "EN 1992-1-1"
    QString title;          ///< Ex: "Calcul des structures en béton — Règles générales et règles pour les bâtiments"
    QString domain;         ///< Ex: "Béton armé", "Acier", "Actions", "Séisme"
    QString justification;  ///< Pourquoi cette norme a été retenue (ex: "Modèle comportant 14 poutres en béton C25/30")
    QString officialUrl;    ///< Lien officiel (JRC / CEN)
};

/**
 * @brief Détecteur automatique des normes et des références bibliographiques.
 * Analyse les éléments, matériaux, sections et résultats réels du projet TSA
 * pour ne citer STRICTEMENT QUE les normes et documents réellement applicables.
 */
class NormativeReferenceDetector
{
public:
    /**
     * @brief Détecte la liste exhaustive des normes appliquées au modèle et aux résultats.
     */
    static std::vector<StandardReference> detectApplicableStandards(const TSA::Model::Model& model);

    /**
     * @brief Génère la webographie et bibliographie officielle et certifiée pour le rapport.
     */
    static std::vector<BibliographicReference> generateBibliography(const TSA::Model::Model& model);
};

} // namespace TSA::NDC
