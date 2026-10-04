#include "ModelRegistry.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace TSA::AI
{

namespace
{
const QStringList kTierOrder = { "tiny", "compact", "standard", "advanced", "powerful" };

constexpr double kComputeBuffersGB = 0.5;  // tampons de calcul llama.cpp (ordre de grandeur)
constexpr double kVramMarginGB = 0.35;     // marge pilote / affichage
constexpr double kSystemReserveGB = 1.5;   // OS + TSA + OpenSees
constexpr double kBandwidthEfficiency = 0.55;
} // namespace

int ModelSpec::tierRank() const
{
    const int i = kTierOrder.indexOf(tier);
    return i < 0 ? 0 : i;
}

QString ModelSpec::downloadUrl() const
{
    if (hfRepo.isEmpty() || hfFile.isEmpty()) return QString();
    return QStringLiteral("https://huggingface.co/%1/resolve/main/%2").arg(hfRepo, hfFile);
}

std::optional<ModelSpec> ModelSpec::fromJson(const QJsonObject& o, QString* error)
{
    ModelSpec m;
    m.id = o.value("id").toString();
    m.displayName = o.value("displayName").toString(m.id);
    m.family = o.value("family").toString();
    m.tier = o.value("tier").toString("standard");
    for (const auto& r : o.value("roles").toArray()) m.roles << r.toString();
    m.parametersB = o.value("parametersB").toDouble();
    m.activeParametersB = o.value("activeParametersB").toDouble(m.parametersB);
    m.quantization = o.value("quantization").toString();
    m.fileSizeBytes = static_cast<qint64>(o.value("fileSizeBytes").toDouble());
    m.sha256 = o.value("sha256").toString().toLower();
    m.layers = o.value("layers").toInt();
    m.kvBytesPerToken = static_cast<qint64>(o.value("kvBytesPerToken").toDouble());
    m.contextDefault = o.value("contextDefault").toInt(8192);
    m.contextMax = o.value("contextMax").toInt(m.contextDefault);
    m.toolCalling = o.value("toolCalling").toBool(false);
    m.hfRepo = o.value("hfRepo").toString();
    m.hfFile = o.value("hfFile").toString();
    m.license = o.value("license").toString();

    if (m.id.isEmpty() || m.fileSizeBytes <= 0 || m.parametersB <= 0.0)
    {
        if (error) *error = QStringLiteral("Entrée de registre invalide (id, fileSizeBytes et parametersB requis) : %1").arg(m.id);
        return std::nullopt;
    }
    if (m.activeParametersB <= 0.0 || m.activeParametersB > m.parametersB) m.activeParametersB = m.parametersB;
    return m;
}

ModelRegistry ModelRegistry::fromJson(const QByteArray& json, QString* error)
{
    ModelRegistry reg;
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
    {
        if (error) *error = QStringLiteral("Registre de modèles illisible : %1").arg(pe.errorString());
        return reg;
    }
    for (const auto& v : doc.object().value("models").toArray())
    {
        QString e;
        if (auto m = ModelSpec::fromJson(v.toObject(), &e)) reg.m_models.push_back(*m);
        else if (error && error->isEmpty()) *error = e;
    }
    return reg;
}

ModelRegistry ModelRegistry::loadDefault(QString* error)
{
    const QString userFile = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/ai/model_registry.json";
    for (const QString& path : { userFile, QStringLiteral(":/ai/model_registry.json") })
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        ModelRegistry reg = fromJson(f.readAll(), error);
        if (!reg.isEmpty()) return reg;
    }
    if (error && error->isEmpty()) *error = QStringLiteral("Aucun registre de modèles disponible.");
    return {};
}

const ModelSpec* ModelRegistry::find(const QString& id) const
{
    for (const auto& m : m_models)
        if (m.id == id) return &m;
    return nullptr;
}

const ModelSpec* ModelRegistry::findByFileName(const QString& fileName) const
{
    for (const auto& m : m_models)
        if (m.hfFile.compare(fileName, Qt::CaseInsensitive) == 0) return &m;
    return nullptr;
}

QString executionModeLabel(ExecutionMode mode)
{
    switch (mode)
    {
    case ExecutionMode::LocalGPU: return QStringLiteral("Local GPU");
    case ExecutionMode::LocalHybrid: return QStringLiteral("Local CPU + GPU");
    case ExecutionMode::LocalCPU: return QStringLiteral("Local CPU");
    case ExecutionMode::CloudSuggested: return QStringLiteral("Cloud proposé");
    }
    return QString();
}

// -----------------------------------------------------------------------------
// Estimations (heuristiques documentées : ordres de grandeur, pas des mesures)
// -----------------------------------------------------------------------------

double ModelSelector::estimateGpuBandwidthGBs(const GpuDevice& gpu)
{
    // Génération limitée par la bande passante mémoire. Sans base de données de cartes,
    // on s'appuie sur la capacité VRAM, corrélée à la gamme (et donc au bus mémoire).
    double bw = 150.0;
    if (gpu.totalGB > 4.5) bw = 250.0;
    if (gpu.totalGB > 8.5) bw = 360.0;
    if (gpu.totalGB > 12.5) bw = 450.0;
    if (gpu.totalGB > 17.0) bw = 700.0;
    if (gpu.vendor == "Intel") bw *= 0.8;
    return bw;
}

double ModelSelector::bandwidthFromMeasurement(double tokensPerSec, const ModelSpec& model)
{
    const double activeWeights = model.fileSizeGB() * (model.activeParametersB / model.parametersB);
    return tokensPerSec * activeWeights / kBandwidthEfficiency;
}

double ModelSelector::estimateCpuBandwidthGBs(const AIHardwareProfile& hw)
{
    // Bande passante effective DDR4/DDR5 double canal ≈ 20 Go/s ; un CPU sans AVX2 ou avec peu de
    // cœurs devient limité par le calcul : on module par l'indice CPU.
    return 20.0 * std::clamp(hw.cpuScore, 0.3, 1.3);
}

std::optional<ModelRecommendation> ModelSelector::evaluate(const AIHardwareProfile& hw, const ModelSpec& model,
                                                           const SelectionOptions& options)
{
    if (options.requireToolCalling && !model.toolCalling) return std::nullopt;

    const double weights = model.fileSizeGB();
    const double activeWeights = weights * (model.activeParametersB / model.parametersB);
    const double ramBudget = std::max(0.0, std::min(hw.ramAvailableGB > 0 ? hw.ramAvailableGB : hw.ramGB, hw.ramGB * 0.75) - kSystemReserveGB);
    const GpuDevice* gpu = hw.bestDiscreteGpu();
    auto measured = [&](const QString& device, double fallback) {
        const auto it = hw.measuredBandwidthGBs.find(device);
        return (it != hw.measuredBandwidthGBs.end() && it->second > 0.0) ? it->second : fallback;
    };
    const double cpuBw = measured(QStringLiteral("CPU"), estimateCpuBandwidthGBs(hw));

    std::optional<ModelRecommendation> best;
    std::vector<int> contexts = { model.contextDefault };
    if (model.contextDefault > 4096) contexts.push_back(4096);

    for (int ctx : contexts)
    {
        const double kv = model.kvCacheGB(ctx);
        auto consider = [&](ModelRecommendation r) {
            if (!best || r.estimatedTokensPerSec > best->estimatedTokensPerSec + 0.5) best = r;
        };

        if (gpu)
        {
            const double gpuAvail = gpu->freeGB - kVramMarginGB;
            const double gpuBw = measured(gpu->deviceId, estimateGpuBandwidthGBs(*gpu));
            // 1. Tout en VRAM
            if (weights + kv + kComputeBuffersGB <= gpuAvail)
            {
                ModelRecommendation r;
                r.mode = ExecutionMode::LocalGPU;
                r.backend = gpu->backend;
                r.deviceId = gpu->deviceId;
                r.contextSize = ctx;
                r.gpuShare = 1.0;
                r.memoryRequiredGB = weights + kv + kComputeBuffersGB;
                r.estimatedTokensPerSec = kBandwidthEfficiency * gpuBw / activeWeights;
                r.reasons << QStringLiteral("Le modèle (%1 Go) et son contexte tiennent dans la VRAM libre de %2 (%3 Go).")
                                 .arg(weights, 0, 'f', 1).arg(gpu->name).arg(gpu->freeGB, 0, 'f', 1);
                consider(r);
            }
            // 2. Hybride : le reste des couches sur CPU
            const double gpuWeights = gpuAvail - kv - kComputeBuffersGB;
            if (gpuWeights > 0.15 * weights && gpuWeights < weights && weights - gpuWeights <= ramBudget)
            {
                const double share = gpuWeights / weights;
                const double secPerToken = activeWeights * (share / gpuBw + (1.0 - share) / cpuBw);
                ModelRecommendation r;
                r.mode = ExecutionMode::LocalHybrid;
                r.backend = gpu->backend;
                r.deviceId = gpu->deviceId;
                r.contextSize = ctx;
                r.gpuShare = share;
                r.memoryRequiredGB = weights + kv + kComputeBuffersGB;
                r.estimatedTokensPerSec = kBandwidthEfficiency / secPerToken;
                r.reasons << QStringLiteral("VRAM insuffisante pour tout le modèle : ≈%1 % des couches sur %2, le reste sur CPU.")
                                 .arg(std::round(share * 100.0)).arg(gpu->name);
                consider(r);
            }
        }
        // 3. CPU seul
        if (weights + kv + kComputeBuffersGB <= ramBudget)
        {
            ModelRecommendation r;
            r.mode = ExecutionMode::LocalCPU;
            r.backend = QStringLiteral("CPU");
            r.contextSize = ctx;
            r.gpuShare = 0.0;
            r.memoryRequiredGB = weights + kv + kComputeBuffersGB;
            r.estimatedTokensPerSec = kBandwidthEfficiency * cpuBw / activeWeights;
            r.reasons << QStringLiteral("Exécution CPU (%1 cœurs%2), %3 Go de RAM utilisables.")
                             .arg(hw.physicalCores).arg(hw.avx2 ? QStringLiteral(", AVX2") : QString())
                             .arg(ramBudget, 0, 'f', 1);
            consider(r);
        }
        // Le contexte complet est prioritaire sur la vitesse : un assistant à outils a besoin
        // de ~8k jetons (consignes + contexte d'ingénierie + outils + réponse). Le contexte réduit
        // n'est qu'un dernier recours, quand rien ne tient avec le contexte complet.
        if (best) break;
    }

    if (best)
    {
        if (best->contextSize < model.contextDefault)
        {
            best->limitedResources = true;
            best->reasons << QStringLiteral("Contexte réduit à %1 jetons pour tenir en mémoire : analyses longues limitées.").arg(best->contextSize);
        }
    }
    return best;
}

ModelRecommendation ModelSelector::recommend(const AIHardwareProfile& hw, const ModelRegistry& registry,
                                             const SelectionOptions& options)
{
    struct Candidate { const ModelSpec* spec; ModelRecommendation rec; };
    std::vector<Candidate> candidates;
    for (const auto& m : registry.models())
    {
        if (!options.requiredRole.isEmpty() && !m.roles.contains(options.requiredRole)) continue;
        if (auto r = evaluate(hw, m, options))
        {
            r->model = &m;
            candidates.push_back({ &m, *r });
        }
    }

    auto pickBest = [&](double minSpeed) -> const Candidate* {
        const Candidate* best = nullptr;
        for (const auto& c : candidates)
        {
            if (c.rec.estimatedTokensPerSec < minSpeed) continue;
            if (!best) { best = &c; continue; }
            // Priorité : contexte complet, puis gamme du modèle, puis qualité de quantification.
            const auto key = [](const Candidate& x) {
                return std::make_tuple(x.rec.contextSize >= x.spec->contextDefault, x.spec->tierRank(), x.spec->fileSizeBytes);
            };
            if (key(c) > key(*best)) best = &c;
        }
        return best;
    };

    if (const Candidate* c = pickBest(options.minInteractiveTokensPerSec))
    {
        ModelRecommendation r = c->rec;
        r.reasons.prepend(QStringLiteral("Modèle le plus capable offrant une vitesse interactive (≈%1 jetons/s estimés).")
                              .arg(r.estimatedTokensPerSec, 0, 'f', 0));
        return r;
    }
    if (const Candidate* c = pickBest(options.minAcceptableTokensPerSec))
    {
        ModelRecommendation r = c->rec;
        r.limitedResources = true;
        r.reasons.prepend(QStringLiteral("Ressources limitées : réponses lentes (≈%1 jetons/s estimés). Le Cloud reste disponible en option.")
                              .arg(r.estimatedTokensPerSec, 0, 'f', 1));
        return r;
    }
    if (!candidates.empty())
    {
        const auto it = std::min_element(candidates.begin(), candidates.end(),
                                         [](const Candidate& a, const Candidate& b) { return a.spec->fileSizeBytes < b.spec->fileSizeBytes; });
        ModelRecommendation r = it->rec;
        r.limitedResources = true;
        r.reasons.prepend(QStringLiteral("Machine modeste : modèle compact, contexte réduit. Le Cloud est recommandé pour les analyses longues."));
        return r;
    }

    ModelRecommendation none;
    none.mode = ExecutionMode::CloudSuggested;
    none.limitedResources = true;
    none.reasons << QStringLiteral("Aucun modèle local ne tient dans la mémoire disponible (%1 Go de RAM).").arg(hw.ramGB, 0, 'f', 1);
    return none;
}

} // namespace TSA::AI
