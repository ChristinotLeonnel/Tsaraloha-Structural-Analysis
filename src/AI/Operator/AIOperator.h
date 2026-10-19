#pragma once

// Opérateur IA : exécute les commandes métier du registre central (TSA::Automation::CommandRegistry) demandées par
// le modèle de langage, sur la session du projet ouvert, et VÉRIFIE lui-même le résultat dans le modèle.
//
// Rien n'est contourné : une commande passe par CommandRegistry::execute (validation des arguments, une entrée
// Annuler par commande modifiante, transaction annulée en cas d'échec), exactement comme le serveur MCP.
//
// Politique (liste blanche = registre intégré) :
//   - commandes de lecture (requêtes, résultats, comparaison) : toujours permises ;
//   - commandes modifiantes et calcul : seulement si le mode opérateur est activé par l'utilisateur, et au plus
//     maxActionsPerRequest() par demande (au-delà : refus, nouvelle demande explicite nécessaire) ;
//   - commande inconnue, arguments invalides, aucun projet : refus explicite, rien n'est exécuté.
// Aucune commande de suppression ni d'écriture de fichier n'existe dans le registre : l'opérateur ne peut ni
// supprimer d'éléments ni remplacer ou écraser un projet.
//
// Vérification : après chaque commande réussie, l'état réel du modèle est relu (élément créé, extrémités d'une barre,
// type d'appui, cas ou charge ajoutés, résultats publiés et à jour). Le résultat rendu au modèle de langage distingue
// « exécuté » et « vérifié » ; une vérification qui échoue n'est jamais présentée comme un succès.

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace TSA::Project
{
class ProjectSession;
}

namespace TSA::AI
{

/// Une action de l'opérateur, telle qu'elle est journalisée et affichée (jamais de clé ni de contenu de fichier).
struct OperatorAction
{
    qint64 timestampMs = 0;
    QString command;          ///< identifiant du registre (« model.create_node »)
    QJsonObject arguments;    ///< arguments reçus du modèle de langage
    bool modifies = false;    ///< commande modifiante ou calcul
    bool refused = false;     ///< refusée par la politique (rien n'a été exécuté)
    bool executed = false;    ///< exécutée avec succès par le registre
    bool verified = false;    ///< état du modèle relu et conforme
    QString message;          ///< message du registre ou motif du refus
    QJsonObject outputs;      ///< sorties de la commande (id créé, valeurs calculées…)
    QJsonObject verification; ///< ce qui a été relu et constaté
    QJsonObject toJson() const;
};

class AIOperator
{
public:
    using SessionProvider = std::function<TSA::Project::ProjectSession*()>;
    using ActionObserver = std::function<void(const OperatorAction&)>;

    explicit AIOperator(SessionProvider session);

    /// Mode opérateur : autorise les commandes modifiantes et le calcul (désactivé par défaut).
    void setActEnabled(bool enabled) { m_actEnabled = enabled; }
    bool actEnabled() const { return m_actEnabled; }
    void setMaxActionsPerRequest(int count) { m_maxActions = count; }
    int maxActionsPerRequest() const { return m_maxActions; }

    /// Nouvelle demande de l'utilisateur : remet à zéro le compteur d'actions.
    void beginRequest() { m_actionsThisRequest = 0; }

    /// Observateur appelé après chaque action (panneau IA, journal, rafraîchissement des vues si modifiante).
    void setObserver(ActionObserver observer) { m_observer = std::move(observer); }

    /// Exécute une commande du registre (arguments JSON) ; rend le résultat structuré destiné au modèle de langage.
    OperatorAction execute(const QString& command, const QJsonObject& arguments);

    const std::vector<OperatorAction>& journal() const { return m_journal; }
    void clearJournal() { m_journal.clear(); }

    /// Signatures compactes des commandes du registre, pour la description de l'outil (petits modèles locaux).
    static QString commandCatalog();
    /// Identifiants des commandes intégrées (énumération JSON de l'outil).
    static QStringList commandIds();
    /// Vrai pour une commande qui modifie le modèle ou lance un calcul.
    static bool isModifying(const QString& command);

private:
    OperatorAction finish(OperatorAction action);

    SessionProvider m_session;
    ActionObserver m_observer;
    bool m_actEnabled = false;
    int m_maxActions = 40;
    int m_actionsThisRequest = 0;
    std::vector<OperatorAction> m_journal;
};

} // namespace TSA::AI
