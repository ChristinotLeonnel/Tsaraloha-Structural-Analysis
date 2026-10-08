#pragma once

// Éditeur de Blueprints (composant partagé, couche widgets) : palette de nœuds, scène, valeurs du
// nœud sélectionné, actions Nouveau / Ouvrir / Enregistrer / Valider / Exécuter, historique du graphe
// (Annuler / Rétablir propres à l'éditeur), débogueur (points d'arrêt, pas à pas, valeurs produites,
// arrêt), pont texte (script de commandes, description pour l'IA).
// Il exécute le Blueprint sur la session de projet fournie (setSession) : les commandes passent par le
// registre central, donc par l'historique Annuler / Rétablir du projet et les observateurs du modèle.
// Un graphe qui n'accède pas au projet (calcul, science) s'exécute en tâche de fond : l'interface reste
// réactive et l'exécution peut être arrêtée. Un graphe qui modifie le projet reste dans le thread de
// l'interface (les vues observent le modèle) ; le débogage aussi (pause = boucle d'événements locale).

#include "Blueprint/BlueprintRuntime.h"

#include <QWidget>

#include <memory>
#include <set>
#include <vector>

class QAction;
class QEventLoop;
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
    /// Exécute avec le débogueur : arrêt aux points d'arrêt (ou au premier nœud d'action s'il n'y en a aucun).
    bool debug();
    void validate();

    // Débogueur
    /// Nœud sélectionné seul dans la scène (0 sinon).
    int selectedNode() const;
    void toggleBreakpoint(int node);
    const std::set<int>& breakpoints() const { return m_breakpoints; }
    bool isRunning() const { return m_running; }
    bool isPaused() const { return m_pauseLoop != nullptr; }
    void continueExecution();
    void step();
    void stop();

    // Historique du graphe (indépendant de l'historique du projet)
    void undo();
    void redo();
    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }

    // Pont texte (IA, scripts)
    bool importScript(const QString& script, QString* error = nullptr);
    QString exportScript(QStringList* warnings = nullptr) const;
    QString describeGraph() const;

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
    bool execute(bool debugging, bool stepFromStart);
    TSA::Blueprint::DebugAction pause(const TSA::Blueprint::DebugState& state);
    void showWatch(const std::map<int, std::map<std::string, TSA::Blueprint::Value>>* outputs);
    void setRunning(bool running);
    void recordChange(const QString& coalesceKey = {});
    void resetHistory();
    void updateActions();

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
    QTreeWidget* m_watch = nullptr;

    std::set<int> m_breakpoints;
    TSA::Blueprint::BreakpointDebugger* m_debugger = nullptr;   ///< pendant une exécution déboguée
    TSA::Blueprint::Runner* m_runner = nullptr;                 ///< pendant une exécution
    QEventLoop* m_pauseLoop = nullptr;                          ///< non nul : en pause
    bool m_running = false;

    std::vector<TSA::Blueprint::Graph> m_undo, m_redo;
    TSA::Blueprint::Graph m_snapshot;                           ///< état après la dernière modification enregistrée
    QString m_lastKey;

    QAction* m_actUndo = nullptr;
    QAction* m_actRedo = nullptr;
    QAction* m_actRun = nullptr;
    QAction* m_actDebug = nullptr;
    QAction* m_actStep = nullptr;
    QAction* m_actContinue = nullptr;
    QAction* m_actStop = nullptr;
    QAction* m_actBreakpoint = nullptr;
};

} // namespace TSA::UI
