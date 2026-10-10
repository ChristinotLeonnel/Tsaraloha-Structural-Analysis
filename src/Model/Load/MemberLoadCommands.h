#pragma once

// Application des charges sur barres au modèle (sans interface) : validation de chaque cible,
// tout ou rien, une seule entrée Annuler, détection des doublons. Utilisé par la fenêtre de
// charge sur barre ; testable sans widget.

#include "MemberLoad.h"

#include <string>
#include <vector>

namespace TSA::Model
{

class Model;

struct MemberLoadTarget
{
    int elementId = 0;
    MemberTargetType type = MemberTargetType::Beam;
};

/// Longueur de la barre cible (m), ou -1 si elle n'existe pas.
double memberLoadTargetLength(const Model& model, int elementId, MemberTargetType type);

/// Vrai si une charge identique (élément, cas, type, direction, repère, intensités, intervalle) existe
/// déjà : protège contre une double application involontaire.
bool hasEquivalentMemberLoad(const Model& model, const MemberLoad& load);

/// Applique le prototype (intensités, direction, intervalle, cas) à chaque cible. Tout ou rien : si
/// une cible est introuvable ou la charge invalide sur l'une d'elles, aucune charge n'est ajoutée et
/// *error décrit la première faute. Sinon une seule entrée Annuler, une notification par charge ;
/// retourne les identifiants créés.
std::vector<int> applyMemberLoad(Model& model, const MemberLoad& prototype,
                                 const std::vector<MemberLoadTarget>& targets,
                                 const std::string& undoLabel, std::string* error = nullptr);

} // namespace TSA::Model
