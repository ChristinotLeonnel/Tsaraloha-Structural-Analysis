#pragma once

// Serveur d'automatisation local (TSA, TSALab) : donne accès au projet ouvert à un client local — en
// pratique le pont MCP `tsaraloha-mcp.exe`, par lequel Claude Code (ou tout client MCP) devient
// co-ingénieur. Canal nommé local réservé au compte Windows de l'utilisateur (QLocalServer, accès
// utilisateur seulement) ; aucun port réseau.
//
// Tout passe par l'existant, rien n'est contourné :
//   - commandes du registre central (une entrée Annuler par commande modifiante, vues notifiées) ;
//   - scripts de commandes → Blueprint (BlueprintScript) exécutés sur la session ;
//   - outils de LECTURE de l'assistant IA (AIToolRegistry) et contexte d'ingénierie structuré ;
//   - Annuler / Rétablir de la session.
//
// Protocole : une requête JSON par ligne {"id", "method", "params"} → une réponse par ligne
// {"id", "result"} ou {"id", "error": {"message"}}. Traitement dans le thread de l'interface (signaux
// du QLocalServer), comme une action de l'utilisateur.
//
// Plusieurs documents (onglets) : un serveur par document, un seul à l'écoute — celui du document au premier plan
// (nom de canal fixe, compatibilité des clients). Au changement d'onglet, le serveur du document quitté est arrêté et
// ses connexions fermées (sockets enfants du QLocalServer) : le pont se reconnecte au document actif, jamais à un
// document caché ou fermé. Chaque réponse porte « document » {id stable, nom, chemin} ; une requête peut fixer
// params.document_id : s'il ne désigne pas ce document actif, erreur explicite, rien n'est exécuté (docs/MCP.md).

#include "../AI/Context/EngineeringContext.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class QLocalServer;
class QLocalSocket;

namespace TSA::Project
{
class ProjectSession;
}

namespace TSA::Automation
{

class AutomationServer : public QObject
{
    Q_OBJECT

public:
    explicit AutomationServer(TSA::Project::ProjectSession* session, QObject* parent = nullptr);
    ~AutomationServer() override;

    /// Nom du canal : « tsaraloha-<produit> » (ex. tsaraloha-tsalab). Utilisé aussi par le pont MCP.
    static QString defaultName();
    /// Écoute sur le canal. Faux si une autre instance l'occupe déjà (*error reçoit la raison).
    bool start(const QString& name = defaultName(), QString* error = nullptr);
    void stop();
    bool isListening() const;
    QString serverName() const;

    /// Sources du contexte d'ingénierie (résultats, sélection, nom du projet) ; par défaut : modèle seul.
    void setSourcesProvider(std::function<TSA::AI::EngineeringSources()> provider) { m_sources = std::move(provider); }
    /// Nom et chemin du projet (réponse à « hello »).
    void setProjectInfoProvider(std::function<QJsonObject()> provider) { m_projectInfo = std::move(provider); }
    /// Identifiant stable du document servi (constant pendant toute la vie de l'onglet).
    void setDocumentId(const QString& id) { m_documentId = id; }
    QString documentId() const { return m_documentId; }
    /// Le document est-il encore ouvert et au premier plan ? Vérifié à chaque requête (par défaut : oui).
    void setActiveCheck(std::function<bool()> check) { m_activeCheck = std::move(check); }
    /// Documents ouverts [{id, name, path, active}] (méthode list_documents) ; par défaut : ce document seul.
    void setDocumentsProvider(std::function<QJsonArray()> provider) { m_documents = std::move(provider); }

    /// Traite une requête (public pour les tests). Méthodes : hello, list_commands, execute, execute_line,
    /// run_script, model_summary, read_tool, undo, redo.
    QJsonObject handle(const QJsonObject& request);

signals:
    /// Le projet a été modifié par un client (actualiser Annuler / Rétablir, titre, vues).
    void projectModified();
    /// Message pour la console de l'application.
    void logMessage(const QString& text, const QString& type);

private:
    void onNewConnection();
    void onReadyRead(QLocalSocket* socket);
    TSA::AI::EngineeringSources sources() const;

    TSA::Project::ProjectSession* m_session = nullptr;
    QLocalServer* m_server = nullptr;
    std::function<TSA::AI::EngineeringSources()> m_sources;
    std::function<QJsonObject()> m_projectInfo;
    QString m_documentId;
    std::function<bool()> m_activeCheck;
    std::function<QJsonArray()> m_documents;
    QJsonObject documentJson() const;
};

} // namespace TSA::Automation
