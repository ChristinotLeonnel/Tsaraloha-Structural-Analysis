#pragma once

// Blueprint — programmation visuelle de l'écosystème Tsaraloha (ADR-024, phase 6).
//
// Un Blueprint est un graphe de nœuds reliés par deux sortes de liens :
//   - EXÉCUTION (flèches blanches) : ordre des actions (créer, charger, calculer…) ;
//   - DONNÉES (liens colorés par type) : valeurs passées d'une sortie à une entrée.
// Nœuds « purs » (math, logique, paramètres) : sans broche d'exécution, évalués à la demande.
// Nœuds « d'action » : déclenchés par le flux d'exécution ; chaque commande du registre central
// (TSA::Automation::CommandRegistry) est automatiquement un nœud d'action.
// Valeurs : booléen, entier, réel, texte, point, liste d'identifiants, et objets structurés immuables
// (TSA::Automation::ObjectValue : matrice, modèle de calcul, résultat…) dont le type (PinSpec::objectType)
// est contrôlé à la connexion et à l'exécution.
//
// Couche modèle : types C++ standard (valeurs = TSA::Automation::Value), aucun widget.

#include "../Automation/CommandRegistry.h"

#include <memory>
#include <string>
#include <vector>

namespace TSA::Blueprint
{

using TSA::Automation::ObjectRef;
using TSA::Automation::ObjectValue;
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
    std::string objectType;      ///< ValueType::Object : type d'objet (« Matrix »…) ; vide = tout objet
};

/// Statut d'un composant de la bibliothèque.
enum class NodeStatus
{
    Standard,       ///< nœud de l'éditeur (paramètres, maths, flux, commandes)
    Validated,      ///< composant scientifique validé : logique testée contre des solutions de référence
    Experimental    ///< composant expérimental : pédagogie, recherche, composant de l'utilisateur ; non validé
};

const char* statusName(NodeStatus status);   ///< « standard », « validé », « expérimental »

struct NodeDefinition
{
    std::string id;              ///< « math.add », « flow.for », « cmd.model.create_node »
    std::string title;
    std::string category;        ///< Core, Math, Logic, Flow, Project, Model, Loads, TSALab, Debug…
    std::string description;
    std::vector<PinSpec> inputs;
    std::vector<PinSpec> outputs;
    bool pure = false;           ///< sans broche d'exécution : évalué à la demande
    /// Lit ou modifie le projet (commandes du registre) : le graphe doit alors s'exécuter dans le thread de
    /// l'interface (les vues observent le modèle). Sinon, il peut s'exécuter en tâche de fond.
    bool usesProject = false;
    /// Nœud pur dont les sorties ne dépendent QUE de ses entrées (ni projet, ni variable, ni hasard) : ses résultats
    /// sont réutilisés tant que ses entrées sont identiques, dans une exécution et d'une exécution à l'autre
    /// (EvaluationCache). Ignoré pour un nœud d'action ou qui utilise le projet.
    bool cacheable = false;
    NodeStatus status = NodeStatus::Standard;
    /// Version du type de nœud, enregistrée dans les fichiers : un fichier créé avec une version plus récente est signalé.
    int version = 1;
    /// Documentation détaillée : formules, hypothèses, unités, repères, limites de validité.
    std::string documentation;
    /// Implémentation : fichier source (relatif au dépôt) et fonctions, pour consulter la logique d'un composant pré-codé.
    std::string source;
    /// Sorties d'exécution déclenchées dans l'ordre (séquence, boucle : corps puis « terminé ») : ce qui suit une sortie
    /// s'exécute après ce qui suit les précédentes (contrôle des dépendances entre nœuds d'action).
    bool orderedOutputs = false;
    /// Point de départ de l'exécution (« Début », entrée d'un sous-graphe) : Runner::run exécute chacun de ces nœuds.
    bool entry = false;
    /// Révision du contenu d'un type défini dans le graphe (sous-graphe : son corps et ceux des sous-graphes qu'il appelle,
    /// par identifiant) : un résultat mis en cache n'est réutilisé que pour la même révision — sinon une définition modifiée
    /// (annuler, import, interface) rendrait le résultat de l'ancienne. Comparaison par identité : les corps sont immuables
    /// et l'entrée du cache les garde en vie. Vide pour les nœuds natifs.
    std::vector<std::shared_ptr<const void>> revision;

    const PinSpec* input(const std::string& name) const;
    const PinSpec* output(const std::string& name) const;
};

/// Compatibilité d'un lien de données (Entier → Réel admis ; générique accepte tout ; objets : même type d'objet,
/// ou broche « tout objet »).
bool dataCompatible(const PinSpec& from, const PinSpec& to);
/// Type affiché d'une broche (« réel », « Matrix », « générique »), dans la langue de l'interface.
std::string pinTypeName(const PinSpec& pin);
/// Titre, catégorie, description ou libellé de broche d'une définition dans la langue de l'interface. Les définitions
/// gardent le texte source français (marqué QT_TRANSLATE_NOOP, contexte « TSA::Blueprint::Nodes » ou, pour les nœuds des
/// commandes, « TSA::Automation::Commands ») : il sert de clé (couleur des catégories…). Texte inconnu : rendu tel quel.
std::string displayText(const std::string& source);

} // namespace TSA::Blueprint
