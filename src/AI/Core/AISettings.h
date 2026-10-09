#pragma once

// Réglages de l'IA (QSettings, groupe « AI »). Par défaut : LOCAL, Cloud toujours soumis à accord,
// fichiers projet jamais envoyés, contenu des échanges jamais journalisé.

#include <QString>

namespace TSA::AI
{

enum class AIMode { Local, Cloud, Auto };

QString aiModeKey(AIMode m);
AIMode aiModeFromKey(const QString& key);

struct CloudSettings
{
    QString preset = QStringLiteral("openai-compatible"); // openai-compatible, gemini, anthropic, ollama
    QString baseUrl;
    QString model;
    bool hasApiKey = false; // la clé elle-même n'est lue qu'à la demande (SecretStore)
};

struct AISettings
{
    AIMode mode = AIMode::Local;

    // Confidentialité
    bool alwaysAskBeforeCloud = true;
    bool neverSendProjectFiles = true;
    bool autoAllowCloud = false;
    bool cloudConsentGiven = false; // accord donné une fois (si alwaysAskBeforeCloud = false)
    bool logPromptContent = false;

    // Local
    QString llamaServerPath;  // vide = détection automatique
    QString modelPath;        // modèle par défaut (vide = recommandation)
    QString modelId;
    QString deviceId;         // vide = recommandation ; « none » = CPU imposé
    int contextSize = 0;      // 0 = recommandation
    bool autoStartLocal = false; // démarrer le moteur local au lancement de TSA
    bool setupCompleted = false;

    CloudSettings cloud;

    static AISettings load();
    void save() const;

    static QString loadApiKey();
    static void storeApiKey(const QString& key); // chiffrée (DPAPI sous Windows)
};

/// Chiffrement lié au compte Windows (DPAPI). Ailleurs : stockage brut (signalé à l'utilisateur).
namespace SecretStore
{
QByteArray protect(const QByteArray& plain);
QByteArray unprotect(const QByteArray& cipher);
bool isProtected();
}

} // namespace TSA::AI
