#pragma once

// Registre des modèles IA locaux (resources/ai/model_registry.json, surchargeable par l'utilisateur)
// et sélection automatique selon le profil matériel. Aucun nom de modèle n'est codé en dur ailleurs.

#include "../Hardware/HardwareProfiler.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

namespace TSA::AI
{

struct ModelSpec
{
    QString id;
    QString displayName;
    QString family;
    QString tier;              // tiny < compact < standard < advanced < powerful
    QStringList roles;         // fast, engineering, reasoning, vision, coding…
    double parametersB = 0.0;
    double activeParametersB = 0.0; // MoE : paramètres actifs par token
    QString quantization;
    qint64 fileSizeBytes = 0;
    QString sha256;
    int layers = 0;
    qint64 kvBytesPerToken = 0;
    int contextDefault = 8192;
    int contextMax = 8192;
    bool toolCalling = false;
    QString hfRepo;
    QString hfFile;
    QString license;

    double fileSizeGB() const { return static_cast<double>(fileSizeBytes) / (1024.0 * 1024.0 * 1024.0); }
    double kvCacheGB(int contextTokens) const
    {
        return static_cast<double>(kvBytesPerToken) * contextTokens / (1024.0 * 1024.0 * 1024.0);
    }
    int tierRank() const;
    QString downloadUrl() const;
    QString label() const { return displayName + " " + quantization; }

    static std::optional<ModelSpec> fromJson(const QJsonObject& o, QString* error = nullptr);
};

class ModelRegistry
{
public:
    /// Registre embarqué (:/ai/model_registry.json), remplacé par <AppData>/ai/model_registry.json s'il existe.
    static ModelRegistry loadDefault(QString* error = nullptr);
    static ModelRegistry fromJson(const QByteArray& json, QString* error = nullptr);

    const std::vector<ModelSpec>& models() const { return m_models; }
    const ModelSpec* find(const QString& id) const;
    const ModelSpec* findByFileName(const QString& fileName) const;
    bool isEmpty() const { return m_models.empty(); }

private:
    std::vector<ModelSpec> m_models;
};

enum class ExecutionMode
{
    LocalGPU,      // modèle entier en VRAM
    LocalHybrid,   // couches réparties GPU + CPU
    LocalCPU,      // CPU uniquement
    CloudSuggested // aucun modèle local raisonnable : le Cloud est proposé (jamais imposé)
};

QString executionModeLabel(ExecutionMode mode);

struct ModelRecommendation
{
    const ModelSpec* model = nullptr; // pointe dans le registre
    ExecutionMode mode = ExecutionMode::CloudSuggested;
    QString backend;      // « CUDA », « Vulkan », « CPU »…
    QString deviceId;     // argument --device, vide pour CPU
    int contextSize = 0;
    double gpuShare = 0.0;          // fraction des poids en VRAM (0 → CPU, 1 → GPU)
    double estimatedTokensPerSec = 0.0;
    double memoryRequiredGB = 0.0;  // poids + cache KV + tampons
    bool limitedResources = false;
    QStringList reasons;            // justification lisible
};

struct SelectionOptions
{
    double minInteractiveTokensPerSec = 8.0; // en-dessous, l'expérience devient pénible
    double minAcceptableTokensPerSec = 3.0;
    QString requiredRole;                    // vide = tout rôle
    bool requireToolCalling = true;
};

class ModelSelector
{
public:
    /// Choisit le modèle le plus capable qui tient en mémoire avec une vitesse interactive ;
    /// sinon le plus capable « acceptable » ; sinon le plus léger qui tient (ressources limitées) ;
    /// sinon CloudSuggested.
    static ModelRecommendation recommend(const AIHardwareProfile& hw, const ModelRegistry& registry,
                                         const SelectionOptions& options = {});

    /// Évalue un modèle précis (utilisé aussi pour un modèle déjà installé choisi par l'utilisateur).
    static std::optional<ModelRecommendation> evaluate(const AIHardwareProfile& hw, const ModelSpec& model,
                                                       const SelectionOptions& options = {});

    /// Bande passante effective déduite d'une mesure : jetons/s × poids actifs / rendement.
    static double bandwidthFromMeasurement(double tokensPerSec, const ModelSpec& model);

    /// Bande passante mémoire estimée (Go/s) — base des estimations de vitesse.
    static double estimateGpuBandwidthGBs(const GpuDevice& gpu);
    static double estimateCpuBandwidthGBs(const AIHardwareProfile& hw);
};

} // namespace TSA::AI
