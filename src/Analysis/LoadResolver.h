#pragma once

#include "../Model/Load/LoadEnums.h"
#include "../Model/Load/MemberLoad.h"
#include "../Model/Load/NodalLoad.h"
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax3.hxx>

namespace TSA::Model
{
class Model;
}

namespace TSA::Analysis
{

class CalculationSnapshot;

/**
 * @brief Structure contenant les composantes locales décomposées d'une charge sur barre.
 * Axe x : longitudinal (start -> end).
 * Axe y : transversal fort/faible.
 * Axe z : transversal orthogonal.
 */
struct LocalMemberLoadComponents
{
    double wx = 0.0; // Axial (kN/m ou kN)
    double wy = 0.0; // Transversal y (kN/m ou kN)
    double wz = 0.0; // Transversal z (kN/m ou kN)
};

using ResolvedMemberLoad = LocalMemberLoadComponents;

/**
 * @brief Service de résolution et projection des charges dans les repères globaux et locaux d'éléments.
 * Convertit les charges définies en repère Global (ex: Gravité -Z) vers les composantes locales
 * requises par les éléments finis OpenSees (eleLoad -type -beamUniform wy wz wx).
 */
class LoadResolver
{
public:
    LoadResolver() = default;

    /**
     * @brief Calcule le repère local orthonormé (LCS) d'une barre définie par deux points et un angle beta.
     */
    static gp_Ax3 computeElementLocalAxes(const gp_Pnt& p1, const gp_Pnt& p2, double betaAngleDeg = 0.0);

    /**
     * @brief Décompose un vecteur de charge globale (ex: 0, 0, -qz) dans le repère local de l'élément.
     */
    static LocalMemberLoadComponents decomposeGlobalVectorToLocal(const gp_Vec& globalVec,
                                                                 const gp_Pnt& p1,
                                                                 const gp_Pnt& p2,
                                                                 double betaAngleDeg = 0.0);

    /**
     * @brief Résout une charge sur barre (MemberLoad) en composantes locales (wx, wy, wz)
     * prêtes pour OpenSees.
     */
    static LocalMemberLoadComponents resolveMemberLoadToLocal(const TSA::Model::MemberLoad& load,
                                                             const TSA::Model::Model& model);

    /**
     * @brief Résout une charge sur barre (MemberLoad) en composantes locales depuis un CalculationSnapshot.
     */
    static LocalMemberLoadComponents resolveMemberLoadToLocal(const TSA::Model::MemberLoad& load,
                                                             const CalculationSnapshot& snapshot);

    static LocalMemberLoadComponents resolveMemberLoadLocal(const TSA::Model::Model& model,
                                                           const TSA::Model::MemberLoad& load)
    {
        return resolveMemberLoadToLocal(load, model);
    }

    /**
     * @brief SEULE définition du sens physique d'une charge sur barre (calcul, rendu, rapports).
     *
     * Convention : la direction n'est retenue que si elle appartient au repère de la charge
     * (axe global en repère global, axe local en repère local) ; q signé donne alors le sens.
     * Sinon (Gravité, ou axe de l'autre repère) la charge est « descendante » de module |q| :
     * −Z global en repère global, −z local en repère local. Ainsi Gravité est toujours vers le bas,
     * quel que soit le signe saisi.
     */
    static bool usesMagnitudeOnly(const TSA::Model::MemberLoad& load);

    /**
     * @brief Vecteur global d'intensité q (kN/m, ou kN pour une charge ponctuelle) appliqué par la
     * charge à une barre p1 → p2 de rotation beta. q = intensité locale (MemberLoad::intensityAt).
     */
    static gp_Vec memberLoadVector(const TSA::Model::MemberLoad& load, double q,
                                   const gp_Pnt& p1, const gp_Pnt& p2, double betaAngleDeg = 0.0);

    /**
     * @brief Composantes locales (wx, wy, wz) de l'intensité q, même convention que memberLoadVector.
     */
    static LocalMemberLoadComponents memberLoadLocalComponents(const TSA::Model::MemberLoad& load, double q,
                                                               const gp_Pnt& p1, const gp_Pnt& p2,
                                                               double betaAngleDeg = 0.0);

    /**
     * @brief Convertit un vecteur local (lx, ly, lz) dans le repère global (WCS).
     */
    static gp_Vec localVectorToGlobal(double lx, double ly, double lz,
                                      const gp_Pnt& p1, const gp_Pnt& p2, double betaAngleDeg = 0.0);
};

} // namespace TSA::Analysis
