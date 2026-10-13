#pragma once

// Graphe d'un Blueprint : nœuds (type, version, position, valeurs saisies), liens (exécution, données) et
// commentaires (cadres titrés qui regroupent visuellement des nœuds : étapes d'un algorithme, annotations).
// Structure de données pure ; la validité des types de nœuds et des liens est vérifiée par la
// bibliothèque (NodeLibrary::validate) et à la connexion (NodeLibrary::canConnect).

#include "BlueprintTypes.h"

#include <map>
#include <optional>

namespace TSA::Blueprint
{

struct NodeInstance
{
    int id = 0;
    std::string type;                     ///< identifiant de NodeDefinition
    double x = 0.0, y = 0.0;              ///< position dans l'éditeur (n'influe pas sur l'exécution)
    std::map<std::string, Value> values;  ///< valeurs saisies des entrées de données non reliées
    int version = 0;                      ///< version du type de nœud à l'enregistrement (0 : version courante)

    bool operator==(const NodeInstance&) const = default;
};

/// Commentaire : cadre titré (texte libre) dans l'éditeur ; déplacer le cadre déplace les nœuds qu'il contient.
/// Sans effet sur l'exécution.
struct Comment
{
    int id = 0;
    double x = 0.0, y = 0.0, width = 400.0, height = 240.0;
    std::string text;
    std::string color = "#546E7A";        ///< couleur du cadre (#RRGGBB)

    bool operator==(const Comment&) const = default;
};

struct Link
{
    int fromNode = 0;
    std::string fromPin;                  ///< sortie
    int toNode = 0;
    std::string toPin;                    ///< entrée

    bool operator==(const Link&) const = default;
};

class Graph
{
public:
    /// Nom et description (métadonnées du fichier .tsbp).
    std::string name;
    std::string description;

    int addNode(const std::string& type, double x = 0.0, double y = 0.0);
    /// Ajoute un nœud avec un identifiant imposé (chargement de fichier). Faux s'il existe déjà.
    bool insertNode(const NodeInstance& node);
    void removeNode(int id);                 ///< supprime aussi ses liens
    NodeInstance* node(int id);
    const NodeInstance* node(int id) const;
    const std::map<int, NodeInstance>& nodes() const { return m_nodes; }

    void addLink(const Link& link);          ///< sans contrôle : passer par NodeLibrary::connect
    void removeLink(const Link& link);
    const std::vector<Link>& links() const { return m_links; }
    /// Lien arrivant sur une entrée (une entrée de donnée n'a qu'une source).
    std::optional<Link> linkTo(int node, const std::string& pin) const;
    /// Liens partant d'une sortie (une sortie d'exécution n'a qu'une cible).
    std::vector<Link> linksFrom(int node, const std::string& pin) const;

    void setValue(int node, const std::string& pin, const Value& value);

    int addComment(const Comment& comment);   ///< identifiant attribué (celui de `comment` s'il est libre et > 0)
    void removeComment(int id);
    Comment* comment(int id);
    const Comment* comment(int id) const;
    const std::map<int, Comment>& comments() const { return m_comments; }
    /// Nœuds dont la position est dans le cadre du commentaire.
    std::vector<int> nodesInComment(int id) const;

    void clear();
    /// Même contenu (nom, description, nœuds, liens, commentaires) ; l'ordre des liens n'est pas significatif.
    bool sameContent(const Graph& other) const;

private:
    std::map<int, NodeInstance> m_nodes;
    std::vector<Link> m_links;
    std::map<int, Comment> m_comments;
    int m_nextId = 1;
    int m_nextComment = 1;
};

} // namespace TSA::Blueprint
