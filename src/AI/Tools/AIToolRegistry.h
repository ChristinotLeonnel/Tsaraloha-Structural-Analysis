#pragma once

// Outils mis à disposition du modèle de langage : liste blanche stricte.
//  - Outils de LECTURE : exécutés immédiatement, lecture seule du modèle TSA.
//  - Outils de PROPOSITION : ne modifient RIEN ; ils produisent une ActionProposal que l'ingénieur
//    accepte ou refuse. Seule l'acceptation déclenche la modification (avec entrée Undo).
// Aucun outil n'exécute de commande système, n'accède librement aux fichiers ni ne supprime de projet.

#include "../Context/EngineeringContext.h"
#include "../../Model/Section.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

namespace TSA::Model { class Model; }

namespace TSA::AI
{

class EngineeringKnowledgeBase;

enum class ToolKind { Read, Proposal };

struct ActionProposal
{
    QString id;
    QString tool;          // outil d'origine
    QString title;         // « Changer la section de B23 : HEA 200 → HEA 240 »
    QString rationale;     // justification donnée par l'IA
    QJsonObject arguments; // arguments validés
    QStringList impacts;   // conséquences connues (résultats invalidés, etc.)
};

struct ToolOutcome
{
    bool ok = false;
    QJsonObject data;
    QString error;
    std::optional<ActionProposal> proposal;
    QJsonObject toModelJson() const; // ce que le LLM reçoit en retour
};

class AIToolRegistry
{
public:
    using SourcesProvider = std::function<EngineeringSources()>;

    AIToolRegistry(SourcesProvider sources, const EngineeringKnowledgeBase* knowledge);

    /// Définitions au format « tools » de l'API Chat Completions.
    QJsonArray toolDefinitions() const;
    static QStringList whitelist();
    static ToolKind kindOf(const QString& tool);

    /// Exécute un outil de la liste blanche. Arguments JSON invalides ou outil inconnu → erreur.
    ToolOutcome execute(const QString& name, const QString& argumentsJson) const;

    /// Applique une proposition ACCEPTÉE par l'ingénieur (modifications du modèle uniquement ;
    /// « run_analysis » est déclenché par l'interface via la commande existante).
    static bool applyProposal(const ActionProposal& proposal, TSA::Model::Model& model, QString* error);

    /// Profils connus de TSA : « IPE 300 », « HEA 240 », « HEB 200 », « UPN 160 », « RECT 300x500 » (mm).
    static std::optional<TSA::Model::Section> parseSection(const QString& text, QString* error = nullptr);

private:
    ToolOutcome proposeSectionChange(const QJsonObject& args, const EngineeringSources& src) const;

    SourcesProvider m_sources;
    const EngineeringKnowledgeBase* m_knowledge = nullptr;
};

} // namespace TSA::AI
