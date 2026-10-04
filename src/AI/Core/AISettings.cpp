#include "AISettings.h"

#include <QSettings>

namespace TSA::AI
{

QString aiModeKey(AIMode m)
{
    switch (m)
    {
    case AIMode::Local: return QStringLiteral("local");
    case AIMode::Cloud: return QStringLiteral("cloud");
    case AIMode::Auto: return QStringLiteral("auto");
    }
    return QStringLiteral("local");
}

AIMode aiModeFromKey(const QString& key)
{
    if (key == "cloud") return AIMode::Cloud;
    if (key == "auto") return AIMode::Auto;
    return AIMode::Local;
}

AISettings AISettings::load()
{
    QSettings s;
    s.beginGroup("AI");
    AISettings a;
    a.mode = aiModeFromKey(s.value("mode", "local").toString());
    a.alwaysAskBeforeCloud = s.value("privacy/alwaysAsk", true).toBool();
    a.neverSendProjectFiles = s.value("privacy/neverSendFiles", true).toBool();
    a.autoAllowCloud = s.value("privacy/autoAllowCloud", false).toBool();
    a.cloudConsentGiven = s.value("privacy/cloudConsent", false).toBool();
    a.logPromptContent = s.value("privacy/logContent", false).toBool();
    a.llamaServerPath = s.value("local/llamaServer").toString();
    a.modelPath = s.value("local/modelPath").toString();
    a.modelId = s.value("local/modelId").toString();
    a.deviceId = s.value("local/deviceId").toString();
    a.contextSize = s.value("local/context", 0).toInt();
    a.autoStartLocal = s.value("local/autoStart", false).toBool();
    a.setupCompleted = s.value("setupCompleted", false).toBool();
    a.cloud.preset = s.value("cloud/preset", "openai-compatible").toString();
    a.cloud.baseUrl = s.value("cloud/baseUrl").toString();
    a.cloud.model = s.value("cloud/model").toString();
    a.cloud.hasApiKey = !s.value("cloud/apiKey").toByteArray().isEmpty();
    s.endGroup();
    return a;
}

void AISettings::save() const
{
    QSettings s;
    s.beginGroup("AI");
    s.setValue("mode", aiModeKey(mode));
    s.setValue("privacy/alwaysAsk", alwaysAskBeforeCloud);
    s.setValue("privacy/neverSendFiles", neverSendProjectFiles);
    s.setValue("privacy/autoAllowCloud", autoAllowCloud);
    s.setValue("privacy/cloudConsent", cloudConsentGiven);
    s.setValue("privacy/logContent", logPromptContent);
    s.setValue("local/llamaServer", llamaServerPath);
    s.setValue("local/modelPath", modelPath);
    s.setValue("local/modelId", modelId);
    s.setValue("local/deviceId", deviceId);
    s.setValue("local/context", contextSize);
    s.setValue("local/autoStart", autoStartLocal);
    s.setValue("setupCompleted", setupCompleted);
    s.setValue("cloud/preset", cloud.preset);
    s.setValue("cloud/baseUrl", cloud.baseUrl);
    s.setValue("cloud/model", cloud.model);
    s.endGroup();
}

QString AISettings::loadApiKey()
{
    QSettings s;
    const QByteArray stored = s.value("AI/cloud/apiKey").toByteArray();
    if (stored.isEmpty()) return QString();
    return QString::fromUtf8(SecretStore::unprotect(QByteArray::fromBase64(stored)));
}

void AISettings::storeApiKey(const QString& key)
{
    QSettings s;
    if (key.isEmpty())
        s.remove("AI/cloud/apiKey");
    else
        s.setValue("AI/cloud/apiKey", SecretStore::protect(key.toUtf8()).toBase64());
}

} // namespace TSA::AI
