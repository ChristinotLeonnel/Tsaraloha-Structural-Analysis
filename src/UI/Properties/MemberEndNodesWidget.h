#pragma once

// Nœuds d'extrémité d'un élément linéaire (poutre, poteau, barre de treillis) dans son panneau de
// propriétés : numéro, coordonnées et appui de chaque nœud, toujours lus dans le modèle (source de
// vérité). L'appui peut être changé d'ici : c'est la propriété du nœud qui est modifiée (une entrée
// Annuler, notification du nœud), et toutes les vues qui le montrent se mettent à jour.

#include <QGroupBox>

class QComboBox;
class QLabel;
class QToolButton;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

class MemberEndNodesWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit MemberEndNodesWidget(QWidget* parent = nullptr);

    void setModel(TSA::Model::Model* model) { m_model = model; }
    /// Libellés des extrémités (« Début / Fin », « Pied / Tête »).
    void setEndLabels(const QString& startLabel, const QString& endLabel);
    /// Nœuds affichés, relus dans le modèle à chaque appel.
    void setNodes(int startNodeId, int endNodeId);
    bool showsNode(int nodeId) const { return nodeId > 0 && (nodeId == m_nodeIds[0] || nodeId == m_nodeIds[1]); }

signals:
    /// Demande d'affichage des propriétés complètes du nœud (orientation, ressorts, DDL…).
    void nodeRequested(int nodeId);

private:
    struct Row
    {
        QLabel* title = nullptr;
        QLabel* node = nullptr;
        QComboBox* support = nullptr;
        QToolButton* open = nullptr;
    };

    void refreshRow(int end);
    void onSupportChosen(int end);

    TSA::Model::Model* m_model = nullptr;
    Row m_rows[2];
    int m_nodeIds[2] = { 0, 0 };
    bool m_loading = false;
};

} // namespace TSA::UI
