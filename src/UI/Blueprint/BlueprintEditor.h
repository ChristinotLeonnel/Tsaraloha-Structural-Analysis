#pragma once

// Éditeur de Blueprints (composant partagé, couche widgets) : palette de nœuds (statut validé / expérimental),
// scène, valeurs du nœud sélectionné, actions Nouveau / Ouvrir / Enregistrer / Valider / Exécuter, historique du graphe
// (Annuler / Rétablir propres à l'éditeur), copier / coller / dupliquer, commentaires, documentation et source des
// nœuds (F1), liste des problèmes (erreurs, avertissements, erreur d'exécution), valeurs produites par la dernière
// exécution (objets ouverts dans leur vue : BlueprintObjectViewer), débogueur (points d'arrêt, pas à pas, valeurs
// produites, arrêt), réutilisation des résultats d'une exécution à l'autre (EvaluationCache), pont texte (script de
// commandes, description pour l'IA).
// Sous-graphes (BlueprintSubgraph.h) : encapsulation de la sélection, édition du corps (pile de graphes, fil d'Ariane ;
// « Retour » enregistre une nouvelle version de la définition et met à jour ses instances), propriétés (nom, catégorie,
// documentation, entrées et sorties typées), fichiers .tsbps (export, import). La bibliothèque effective de l'éditeur
// (bibliothèque standard augmentée des sous-graphes du graphe édité) sert à la scène, à la palette et aux contrôles.
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
class QSplitter;
class QTabWidget;
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

    // Copier / coller (presse-papiers : fragment JSON), commentaires
    void copySelection();
    void paste();
    void duplicateSelection();
    /// Commentaire encadrant les nœuds sélectionnés (ou au centre de la vue).
    int addComment(const QString& text = {});
    void showDocumentation();

    /// Sélectionne exactement ces nœuds dans la scène (automatisation, essais).
    void selectNodes(const std::vector<int>& nodes);

    // Sous-graphes
    /// Remplace les nœuds sélectionnés par l'instance d'un nouveau sous-graphe nommé `name` (demandé si vide).
    bool encapsulateSelection(const QString& name = {}, QString* error = nullptr);
    /// Ouvre le corps du sous-graphe `id` du graphe édité (retour : closeSubgraph).
    bool openSubgraph(const std::string& id, QString* error = nullptr);
    /// Retour au graphe parent. apply : le corps édité (et les définitions créées, importées ou modifiées pendant son
    /// édition) devient une nouvelle version, instances mises à jour, une entrée Annuler dans le parent ; faux (*error) si
    /// le corps est invalide — l'édition continue. apply = faux : modifications abandonnées.
    bool closeSubgraph(bool apply = true, QString* error = nullptr);
    /// Sous-graphe dont le corps est édité (vide : graphe principal).
    std::string currentSubgraph() const;
    int subgraphDepth() const { return static_cast<int>(m_frames.size()); }
    /// Interface du sous-graphe en cours d'édition (nom, catégorie, description, documentation, entrées, sorties) : les
    /// liens du corps vers des broches supprimées ou de type modifié sont retirés (journal). Corps de `updated.id` ouvert.
    bool setSubgraphInterface(const TSA::Blueprint::SubgraphDefinition& updated, QString* error = nullptr);
    /// Boîte de dialogue des propriétés (ouvre d'abord le corps si nécessaire).
    bool editSubgraphProperties(const std::string& id);
    bool exportSubgraph(const std::string& id, const QString& path, QString* error = nullptr);
    /// Définitions d'un fichier .tsbps ajoutées au graphe édité (dépendances d'abord), instances mises à jour. Définition
    /// déjà présente : ignorée si son contenu est identique ; sinon remplacée par celle du fichier (geste délibéré), en
    /// nouvelle version locale — fichier plus ancien et interface qui casse des liens signalés (WARN), une entrée Annuler.
    /// Tout ou rien : une définition invalide n'importe rien.
    bool importSubgraphs(const QString& path, QString* error = nullptr);
    /// Supprime une définition sans instance (graphe principal seulement).
    bool removeSubgraphDefinition(const std::string& id, QString* error = nullptr);
    /// Graphe principal tel qu'il serait enregistré : corps en cours d'édition reportés dans leurs définitions.
    bool composedGraph(TSA::Blueprint::Graph& out, QString* error = nullptr) const;
    /// Bibliothèque effective (standard + sous-graphes du graphe édité).
    const TSA::Blueprint::NodeLibrary& library() const { return m_effective; }

    // Pont texte (IA, scripts)
    bool importScript(const QString& script, QString* error = nullptr);
    QString exportScript(QStringList* warnings = nullptr) const;
    QString describeGraph() const;

    TSA::Blueprint::Graph& graph() { return m_graph; }
    QString currentFile() const { return m_file; }
    /// Rapport de la dernière exécution (valeurs produites, profil, réutilisations).
    const TSA::Blueprint::ExecutionReport& lastReport() const { return m_lastReport; }
    TSA::Blueprint::EvaluationCache& cache() { return m_cache; }

    // Navigation et surface du canevas
    /// Tout le graphe dans la vue (zoom de 5 % à 150 %) ; touche Origine dans le graphe.
    void fitAll();
    /// Nœuds sélectionnés dans la vue, ou tout le graphe sans sélection ; touche F dans le graphe.
    void fitSelection();
    double zoomPercent() const;
    /// Recherche de la palette (Ctrl+F dans le graphe) : palette affichée si elle était repliée.
    void focusNodeSearch();
    /// Nœuds déjà placés dans le graphe édité dont le titre, le type ou le numéro (#12, 12) contient le texte, par
    /// numéro croissant. La recherche de la palette les présente à part (groupe « Dans le graphe », en tête) : les
    /// activer sélectionne et centre le nœud, sans en ajouter.
    std::vector<int> findPlacedNodes(const QString& text) const;
    void setPaletteVisible(bool visible);
    void setInspectorVisible(bool visible);
    void setMinimapVisible(bool visible);
    bool isPaletteVisible() const;
    bool isInspectorVisible() const;
    bool isMinimapVisible() const;
    /// Disposition de l'éditeur (panneaux, mini-carte, tailles) : préférence de l'application, jamais dans le projet.
    QByteArray saveLayoutState() const;
    /// Faux si l'état est illisible : la disposition courante est conservée.
    bool restoreLayoutState(const QByteArray& state);
    QGraphicsView* view() const { return m_view; }

signals:
    /// Message pour la console de l'application (type : SYS, INFO, WARN, ERROR).
    void logMessage(const QString& text, const QString& type);
    /// Le Blueprint a modifié le projet (actions Annuler / Rétablir, vues).
    void projectModified();

private:
    void buildPalette();
    void fitGraph();
    void updateNavigation();
    void filterPalette(const QString& text);
    void refreshPlacedMatches();
    void showNodeProperties();
    void addFromPalette();
    bool execute(bool debugging, bool stepFromStart);
    TSA::Blueprint::DebugAction pause(const TSA::Blueprint::DebugState& state);
    void showWatch(const std::map<int, std::map<std::string, TSA::Blueprint::Value>>* outputs);
    /// Valeurs produites : nœud sélectionné (ou tous) de la dernière exécution, ou de la pause du débogueur.
    void showValues();
    void openValue(QTreeWidgetItem* item);
    void refreshProblems();
    void editComment(int id);
    void graphReplaced();
    void setRunning(bool running);
    void recordChange(const QString& coalesceKey = {});
    void resetHistory();
    void updateActions();
    /// Bibliothèque effective alignée sur les sous-graphes du graphe édité ; vrai si leurs types ont changé (palette
    /// reconstruite ; la scène doit l'être aussi : géométrie des nœuds).
    bool syncLibrary();
    void updateSubgraphBar();
    /// Contrôles du graphe édité (corps : validateBody) et avertissements.
    std::vector<TSA::Blueprint::Issue> issues() const;
    /// Graphe exécuté : le graphe édité ; dans un corps, la définition éditée prend le corps courant (essai du corps seul).
    TSA::Blueprint::Graph graphForRun() const;
    /// « Retour » de l'interface : corps invalide → continuer l'édition ou abandonner les modifications.
    void backToParent();
    /// Sous-graphe de l'instance sélectionnée, sinon celui dont le corps est édité.
    std::string targetSubgraph() const;
    std::string chooseSubgraph(const QString& title, bool unusedOnly);
    QStringList knownObjectTypes() const;

    TSA::Blueprint::Graph m_graph;
    const TSA::Blueprint::NodeLibrary& m_library;
    TSA::Blueprint::NodeLibrary m_effective;                     ///< m_library + sous-graphes de m_graph (scène, palette)
    std::vector<TSA::Blueprint::SubgraphDefinition> m_syncedSubgraphs;
    /// Corps en cours d'édition : un cadre par niveau (graphe parent, son historique, ses points d'arrêt, sa vue).
    struct SubgraphFrame
    {
        std::string id;
        TSA::Blueprint::Graph parent;
        std::vector<TSA::Blueprint::Graph> undo, redo;
        std::set<int> breakpoints;
        double scale = 1.0;
        double centerX = 0.0, centerY = 0.0;
    };
    std::vector<SubgraphFrame> m_frames;
    std::string m_paletteSubgraph;                               ///< corps édité quand la palette a été construite
    QWidget* m_subgraphBar = nullptr;
    QLabel* m_breadcrumb = nullptr;
    TSA::Project::ProjectSession* m_session = nullptr;
    QString m_file;

    BlueprintScene* m_scene = nullptr;
    QGraphicsView* m_view = nullptr;
    QGraphicsView* m_minimap = nullptr;
    QSplitter* m_split = nullptr;                               ///< palette | canevas | inspecteur
    QWidget* m_paletteHost = nullptr;
    QWidget* m_inspectorHost = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QAction* m_actPalette = nullptr;
    QAction* m_actInspector = nullptr;
    QAction* m_actMinimap = nullptr;
    QLineEdit* m_search = nullptr;
    QTreeWidget* m_palette = nullptr;
    QTreeWidgetItem* m_placedMatches = nullptr;   // groupe « Dans le graphe » de la palette (null : absent)
    QWidget* m_propertiesHost = nullptr;
    QFormLayout* m_properties = nullptr;
    QLabel* m_nodeTitle = nullptr;
    QLabel* m_nodeDescription = nullptr;
    QLabel* m_nodeStatus = nullptr;
    QWidget* m_docButton = nullptr;
    QLabel* m_watchTitle = nullptr;
    QTreeWidget* m_watch = nullptr;
    QTreeWidget* m_problems = nullptr;
    QTabWidget* m_sideTabs = nullptr;                           ///< onglets « Valeurs » / « Problèmes »
    TSA::Blueprint::EvaluationCache m_cache;
    TSA::Blueprint::ExecutionReport m_lastReport;
    bool m_hasReport = false;
    std::map<int, std::map<std::string, TSA::Blueprint::Value>> m_pauseValues;   ///< sorties visibles pendant une pause

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
