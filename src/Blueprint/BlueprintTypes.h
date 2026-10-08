#pragma once

// Blueprint — programmation visuelle de l'écosystème Tsaraloha (ADR-024, phase 6).
//
// Un Blueprint est un graphe de nœuds reliés par deux sortes de liens :
//   - EXÉCUTION (flèches blanches) : ordre des actions (créer, charger, calculer…) ;
//   - DONNÉES (liens colorés par type) : valeurs passées d'une sortie à une entrée.
// Nœuds « purs » (math, logique, paramètres) : sans broche d'exécution, évalués à la demande.
// Nœuds « d'action » : déclenchés par le flux d'exécution ; chaque commande du registre central
// (TSA::Automation::CommandRegistry) est automatiquement un nœud d'action.
//
// Couche modèle : types C++ standard (valeurs = TSA::Automation::Value), aucun widget.

#include "../Automation/CommandRegistry.h"

#include <string>
#include <vector>

namespace TSA::Blueprint
{

using TSA::Automation::Point3;
using TSA::Automation::Quantity;
using TSA::Automation::Value;
using TSA::Automation::ValueType;

enum class PinKind
{
    Exec,
    Data
};

struct PinSpec
{
    std::string name;            ///< identifiant stable (sérialisation, liens)
    std::string label;           ///< libellé affiché
    PinKind kind = PinKind::Data;
    ValueType type = ValueType::Real;
    bool any = false;            ///< broche générique (accepte tout type de donnée)
    Quantity quantity = Quantity::None;
    Value defaultValue;          ///< valeur d'une entrée non reliée
    bool required = false;       ///< entrée de donnée obligatoire (reliée ou valeur saisie)
};

struct NodeDefinition
{
    std::string id;              ///< « math.add », « flow.for », « cmd.model.create_node »
    std::string title;
    std::string category;        ///< Core, Math, Logic, Flow, Project, Model, Loads, TSALab, Debug…
    std::string description;
    std::vector<PinSpec> inputs;
    std::vector<PinSpec> outputs;
    bool pure = false;           ///< sans broche d'exécution : évalué à la demande

    const PinSpec* input(const std::string& name) const;
    const PinSpec* output(const std::string& name) const;
};

/// Compatibilité d'un lien de données (Entier → Réel admis ; générique accepte tout).
bool dataCompatible(const PinSpec& from, const PinSpec& to);

} // namespace TSA::Blueprint
