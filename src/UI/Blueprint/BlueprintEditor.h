#pragma once

// Éditeur de Blueprints (composant partagé, couche widgets) : palette de nœuds, scène, valeurs du
// nœud sélectionné, actions Nouveau / Ouvrir / Enregistrer / Valider / Exécuter.
// Il exécute le Blueprint sur la session de projet fournie (setSession) : les commandes passent par le
// registre central, donc par l'historique Annuler / Rétablir et les observateurs du modèle.

#include "Blueprint/BlueprintRuntime.h"

#include <QWidget>

#include <memory>

class QFormLayout;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QTreeWidget;

namespace TSA::Project
{
class ProjectSession;
}

namespace TSA::UI
{

class BlueprintScene;

class BlueprintEditor : public QWidget
{
    Q_OBJECT

public:
    explicit BlueprintEditor(QWidget* parent = nullptr);
    ~BlueprintEditor() override;

    void setSession(TSA::Project::ProjectSession* session) { m_session = session; }

    void newBlueprint();
    bool open(const QString& path);
    /// Remplace le Blueprint édité (exemple, Blueprint généré par l'IA…) ; aucun fichier associé.
    void setGraph(TSA::Blueprint::Graph graph);
    bool save();
    bool saveAs();
    /// Valide puis exécute ; journal et profil affichés. Vrai si l'exécution a réussi.
    bool run();
    void validate();

    TSA::Blueprint::Graph& graph() { return m_graph; }
    QString currentFile() const { return m_file; }

signals:
    /// Message pour la console de l'application (type : SYS, INFO, WARN, ERROR).
    void logMessage(const QString& text, const QString& type);
    /// Le Blueprint a modifié le projet (actions Annuler / Rétablir, vues).
    void projectModified();

private:
    void buildPalette();
    void fitGraph();
    void filterPalette(const QString& text);
    void showNodeProperties();
    void addFromPalette();

    TSA::Blueprint::Graph m_graph;
    const TSA::Blueprint::NodeLibrary& m_library;
    TSA::Project::ProjectSession* m_session = nullptr;
    QString m_file;

    BlueprintScene* m_scene = nullptr;
    QGraphicsView* m_view = nullptr;
    QLineEdit* m_search = nullptr;
    QTreeWidget* m_palette = nullptr;
    QWidget* m_propertiesHost = nullptr;
    QFormLayout* m_properties = nullptr;
    QLabel* m_nodeTitle = nullptr;
    QLabel* m_nodeDescription = nullptr;
};

} // namespace TSA::UI
