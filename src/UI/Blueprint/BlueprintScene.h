#pragma once

// Scène graphique d'un Blueprint (éditeur partagé TSA / TSALab) : représentation de
// TSA::Blueprint::Graph (source de vérité), jamais de données propres à l'affichage hormis la position.
//   - nœuds : en-tête coloré par catégorie, broches d'exécution (triangles) et de données (cercles
//     colorés par type) ; valeur saisie affichée sur les entrées non reliées ;
//   - liens : courbes de Bézier (blanches pour l'exécution, couleur du type pour les données) ;
//   - tirer depuis une broche crée un lien (contrôlé par NodeLibrary::connect) ;
//   - Suppr retire les nœuds / liens / commentaires sélectionnés ;
//   - commentaires : cadres titrés derrière les nœuds ; déplacer un cadre déplace les nœuds qu'il contient, la poignée
//     en bas à droite le redimensionne, un double-clic modifie son texte ;
//   - statut des composants : ✓ validé, ⚗ expérimental (en-tête du nœud) ;
//   - sous-graphes : instances (en-tête vert sapin, « ⧉ », double-clic : ouvrir le corps), entrée et sortie d'un corps ;
//   - après une exécution : profil (exécutions, réutilisations, temps) sur chaque nœud, nœud en erreur surligné ;
//   - débogage : points d'arrêt (pastille rouge), nœud en pause surligné, édition bloquée pendant l'exécution.

#include "Blueprint/BlueprintRuntime.h"

#include <QGraphicsScene>

#include <map>
#include <set>

class QGraphicsPathItem;

namespace TSA::UI
{

class BlueprintNodeItem;
class BlueprintCommentItem;

class BlueprintScene : public QGraphicsScene
{
    Q_OBJECT

public:
    BlueprintScene(TSA::Blueprint::Graph& graph, const TSA::Blueprint::NodeLibrary& library, QObject* parent = nullptr);

    /// Reconstruit tous les éléments depuis le graphe (après chargement, suppression…).
    void rebuild();
    int addNode(const std::string& type, const QPointF& pos);
    void deleteSelection();
    /// Commentaire encadrant `rect` (scène) ; sélectionné.
    int addComment(const QRectF& rect, const QString& text);
    void refreshComment(int id);
    std::set<int> selectedNodes() const;
    std::set<int> selectedComments() const;
    /// Sélectionne exactement ces nœuds et commentaires.
    void selectItems(const std::vector<int>& nodes, const std::vector<int>& comments);
    /// Rend le nœud visible et le sélectionne.
    void focusNode(int id);
    /// Affiche le profil d'une exécution et le nœud en erreur (0 : aucun). Rapport vide : efface.
    void showReport(const TSA::Blueprint::ExecutionReport* report);
    void refreshNode(int id);

    TSA::Blueprint::Graph& graph() { return m_graph; }
    const TSA::Blueprint::NodeLibrary& library() const { return m_library; }
    /// Nœud sélectionné seul (0 sinon).
    int selectedNode() const;

    // Débogage
    void setBreakpoints(const std::set<int>& breakpoints);
    bool hasBreakpoint(int id) const { return m_breakpoints.count(id) > 0; }
    /// Nœud en pause (0 : aucun), surligné et rendu visible.
    void setActiveNode(int id);
    int activeNode() const { return m_activeNode; }
    /// Édition (déplacement, liens, suppression) autorisée ; bloquée pendant une exécution.
    void setEditable(bool editable);
    bool isEditable() const { return m_editable; }

    // Interne (éléments) : position déplacée, broche à une position de scène.
    void nodeMoved(int id, const QPointF& pos);
    QPointF pinPosition(int node, const std::string& pin, bool output) const;
    /// Déplace un nœud (commentaire qui emporte son contenu) sans passer par la souris.
    void moveNodeBy(int id, const QPointF& delta);
    void commentChanged();
    void requestCommentEdit(int id) { emit commentEditRequested(id); }
    void requestNodeOpen(int id) { emit nodeOpenRequested(id); }

signals:
    void message(const QString& text);
    void graphChanged();
    /// Des nœuds ont été déplacés à la souris (fin du glisser).
    void nodesMoved();
    /// Double-clic sur un commentaire : modifier son texte.
    void commentEditRequested(int id);
    /// Double-clic sur un nœud (instance de sous-graphe : ouvrir son corps). À traiter en différé : l'éditeur reconstruit
    /// la scène, donc l'élément qui reçoit le clic.
    void nodeOpenRequested(int id);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void rebuildLinks();
    struct PinHit
    {
        int node = 0;
        std::string pin;
        bool output = false;
    };
    bool pinAt(const QPointF& scenePos, PinHit& hit) const;

    TSA::Blueprint::Graph& m_graph;
    const TSA::Blueprint::NodeLibrary& m_library;
    std::map<int, BlueprintNodeItem*> m_items;
    std::map<int, BlueprintCommentItem*> m_commentItems;
    std::vector<QGraphicsPathItem*> m_links;
    QGraphicsPathItem* m_dragLine = nullptr;
    PinHit m_dragFrom;
    std::set<int> m_breakpoints;
    int m_activeNode = 0;
    bool m_editable = true;
    bool m_moved = false;
};

/// Couleur d'un type de donnée (broches, liens).
QColor pinColor(const TSA::Blueprint::PinSpec& pin);

} // namespace TSA::UI
