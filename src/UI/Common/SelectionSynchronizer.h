#pragma once

// Synchronisation sélection ↔ viewport ↔ arbre du modèle ↔ panneau Propriétés, partagée par les
// applications de l'écosystème (MainWindow de TSA, fenêtre de TSALab) — ADR-024.
//
// Une sélection faite dans l'une des vues est répercutée sur les autres (surbrillance, ligne de
// l'arbre, propriétés de l'élément, sélection ensembliste et édition groupée). Ce qui est propre à
// une fenêtre (barre d'état, boîtes de dialogue de création ouvertes, dock des résultats, actions
// Annuler / Rétablir) passe par les signaux ci-dessous : aucune dépendance vers une fenêtre.

#include <QObject>
#include <QString>

class OccView;

namespace TSA::Model
{
class Model;
}
namespace TSA::Viewer
{
class SelectionManager;
}

namespace TSA::UI
{

class ModelTreeWidget;
class PropertyPanel;
class ViewportContainer;

class SelectionSynchronizer : public QObject
{
    Q_OBJECT

public:
    /// Toutes les vues sont obligatoires sauf `container` (règles du viewport : si absent, le plan
    /// de travail choisi dans l'arbre est appliqué directement au viewport).
    SelectionSynchronizer(TSA::Model::Model* model, TSA::Viewer::SelectionManager* selection, OccView* view,
                          ModelTreeWidget* tree, PropertyPanel* properties, ViewportContainer* container,
                          QObject* parent = nullptr);

signals:
    /// Message pour la barre d'état de la fenêtre.
    void statusMessage(const QString& text);
    /// Poutre / câble sélectionné(e) dans le viewport (ex. recharger un dialogue de création ouvert).
    void beamActivated(int beamId);
    void cableActivated(int cableId);
    /// « Résultats » choisi dans l'arbre.
    void resultsRequested();
    /// Le modèle a été modifié depuis le panneau Propriétés (ex. actualiser Annuler / Rétablir).
    void modelEdited();

private:
    void connectTree();
    void connectViewport();
    void connectProperties();

    TSA::Model::Model* m_model = nullptr;
    TSA::Viewer::SelectionManager* m_selection = nullptr;
    OccView* m_view = nullptr;
    ModelTreeWidget* m_tree = nullptr;
    PropertyPanel* m_properties = nullptr;
    ViewportContainer* m_container = nullptr;
};

} // namespace TSA::UI
