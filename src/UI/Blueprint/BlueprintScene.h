#pragma once

// Scène graphique d'un Blueprint (éditeur partagé TSA / TSALab) : représentation de
// TSA::Blueprint::Graph (source de vérité), jamais de données propres à l'affichage hormis la position.
//   - nœuds : en-tête coloré par catégorie, broches d'exécution (triangles) et de données (cercles
//     colorés par type) ; valeur saisie affichée sur les entrées non reliées ;
//   - liens : courbes de Bézier (blanches pour l'exécution, couleur du type pour les données) ;
//   - tirer depuis une broche crée un lien (contrôlé par NodeLibrary::connect) ;
//   - Suppr retire les nœuds / liens sélectionnés ;
//   - après une exécution : profil (exécutions, temps) sur chaque nœud, nœud en erreur surligné.

#include "Blueprint/BlueprintRuntime.h"

#include <QGraphicsScene>

#include <map>

class QGraphicsPathItem;

namespace TSA::UI
{

class BlueprintNodeItem;

class BlueprintScene : public QGraphicsScene
{
    Q_OBJECT

public:
    BlueprintScene(TSA::Blueprint::Graph& graph, const TSA::Blueprint::NodeLibrary& library, QObject* parent = nullptr);

    /// Reconstruit tous les éléments depuis le graphe (après chargement, suppression…).
    void rebuild();
    int addNode(const std::string& type, const QPointF& pos);
    void deleteSelection();
    /// Affiche le profil d'une exécution et le nœud en erreur (0 : aucun). Rapport vide : efface.
    void showReport(const TSA::Blueprint::ExecutionReport* report);
    void refreshNode(int id);

    TSA::Blueprint::Graph& graph() { return m_graph; }
    const TSA::Blueprint::NodeLibrary& library() const { return m_library; }
    /// Nœud sélectionné seul (0 sinon).
    int selectedNode() const;

    // Interne (éléments) : position déplacée, broche à une position de scène.
    void nodeMoved(int id, const QPointF& pos);
    QPointF pinPosition(int node, const std::string& pin, bool output) const;

signals:
    void message(const QString& text);
    void graphChanged();

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
    std::vector<QGraphicsPathItem*> m_links;
    QGraphicsPathItem* m_dragLine = nullptr;
    PinHit m_dragFrom;
};

/// Couleur d'un type de donnée (broches, liens).
QColor pinColor(const TSA::Blueprint::PinSpec& pin);

} // namespace TSA::UI
