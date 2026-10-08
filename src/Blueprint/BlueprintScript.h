#pragma once

// Pont texte ↔ Blueprint (Blueprints modifiables par l'IA, scripts, console).
//
// Script de commandes = suite de lignes du registre central, exécutées dans l'ordre :
//     # commentaire
//     a = model.create_node position=0,0,0
//     b = model.create_node position=6,0,0
//     model.create_beam start=a.id end=b.id section="IPE 300"
// Une ligne « nom = commande … » nomme la commande ; « nom.sortie » dans une valeur relie cette sortie à
// l'entrée (lien de données du Blueprint). Les autres valeurs sont saisies dans le nœud.
//
// fromCommandScript : script → graphe (Début → une commande par ligne, liens d'exécution et de données),
//                     validé par la bibliothèque ; aucune exécution.
// toCommandScript   : graphe → script, pour la chaîne d'exécution linéaire de commandes partant du Début ;
//                     tout ce qui ne s'exprime pas en script (boucles, maths, paramètres) est signalé.
// describe          : description textuelle d'un graphe (nœuds, valeurs, liens) pour le contexte d'une IA.

#include "BlueprintRuntime.h"

#include <string>
#include <vector>

namespace TSA::Blueprint
{

bool fromCommandScript(const std::string& script, const NodeLibrary& library,
                       const TSA::Automation::CommandRegistry& commands, Graph& out, std::string* error = nullptr);

/// Script de la chaîne de commandes ; *warnings reçoit ce qui n'a pas pu être exprimé.
std::string toCommandScript(const Graph& graph, const NodeLibrary& library, std::vector<std::string>* warnings = nullptr);

std::string describe(const Graph& graph, const NodeLibrary& library);

} // namespace TSA::Blueprint
