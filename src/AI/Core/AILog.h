#pragma once

// Journal IA : métadonnées uniquement (fournisseur, modèle, backend, latence, jetons, erreurs,
// replis). Le contenu des échanges n'est écrit que si l'utilisateur l'a explicitement activé.
// Fichiers JSON Lines : <AppData>/ai/logs/ai-AAAAMMJJ.jsonl

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace TSA::AI
{

struct AILogEntry
{
    QString timestamp;
    QString event;     // request, fallback, error, server, consent, benchmark
    QString provider;
    QString model;
    QString backend;
    qint64 latencyMs = -1;
    int promptTokens = -1;
    int completionTokens = -1;
    double tokensPerSecond = 0.0;
    QString detail;    // message d'erreur ou de repli — jamais de données du projet
    QJsonObject toJson() const;
};

class AILog
{
public:
    static void append(AILogEntry entry);
    static QVector<AILogEntry> recent(int maxEntries = 200);
    static QString directory();
};

} // namespace TSA::AI
