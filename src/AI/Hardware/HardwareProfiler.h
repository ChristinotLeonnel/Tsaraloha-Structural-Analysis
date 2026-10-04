#pragma once

// Profil matériel pour l'IA locale : CPU (cœurs, AVX/AVX2/AVX-512), RAM, GPU et backends.
// Les GPU sont ceux que voit réellement le moteur d'inférence (llama-server --list-devices) :
// aucune hypothèse sur le fabricant (NVIDIA, AMD, Intel) ni sur le backend (CUDA, Vulkan, HIP, SYCL).

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <map>
#include <vector>

namespace TSA::AI
{

struct GpuDevice
{
    QString deviceId;   // identifiant llama.cpp, ex. « Vulkan1 », « CUDA0 » (argument --device)
    QString backend;    // « Vulkan », « CUDA », « HIP », « SYCL », « Metal »…
    QString name;
    QString vendor;     // « NVIDIA », « AMD », « Intel », « Apple », « Unknown »
    double totalGB = 0.0;
    double freeGB = 0.0;
    bool integrated = false; // iGPU : mémoire partagée avec la RAM, exclu de l'offload recommandé
};

struct AIHardwareProfile
{
    // CPU
    QString cpuName;
    int physicalCores = 0;
    int logicalCores = 0;
    bool sse42 = false;
    bool avx = false;
    bool avx2 = false;
    bool fma = false;
    bool avx512 = false;
    double cpuScore = 0.0; // indice relatif (≈ 1.0 pour 4 cœurs AVX2)

    // Mémoire
    double ramGB = 0.0;
    double ramAvailableGB = 0.0;

    // GPU et runtimes
    std::vector<GpuDevice> gpus;
    bool cudaDriver = false;   // nvcuda.dll présent
    bool vulkanLoader = false; // vulkan-1.dll présent
    bool hipRuntime = false;   // amdhip64*.dll présent
    bool metal = false;        // macOS uniquement

    // Moteur d'inférence local
    QString llamaServerPath;
    QString llamaVersion;
    QStringList llamaBackends; // backends compilés dans la distribution llama.cpp trouvée
    QString probeError;        // échec éventuel de la détection (non bloquant)

    // Auto-benchmark : bande passante effective MESURÉE (Go/s) par périphérique (« CPU » ou
    // identifiant llama.cpp). Prioritaire sur les estimations heuristiques.
    std::map<QString, double> measuredBandwidthGBs;

    /// GPU dédié offrant le plus de mémoire libre, ou nullptr.
    const GpuDevice* bestDiscreteGpu() const;
    bool hasDiscreteGpu() const { return bestDiscreteGpu() != nullptr; }
    QJsonObject toJson() const;
};

class HardwareProfiler
{
public:
    /// Détection complète (CPU, RAM, runtimes, puis llama-server --list-devices).
    /// Bloquante (quelques secondes au plus) : à appeler hors du thread UI.
    static AIHardwareProfile probe(const QString& llamaServerPath, int timeoutMs = 15000);

    /// Analyse la sortie de « llama-server --list-devices ». Fonction pure (testée).
    static std::vector<GpuDevice> parseListDevices(const QString& output);

    /// Backends présents dans un dossier llama.cpp (ggml-cuda.dll, ggml-vulkan.dll…).
    static QStringList backendsFromLibraries(const QStringList& fileNames);

    static QString vendorFromName(const QString& gpuName);
    static bool looksIntegrated(const QString& vendor, const QString& gpuName);
    static double computeCpuScore(int physicalCores, bool avx2, bool avx512);

    /// Recherche llama-server : chemin configuré, <app>/ai/llama, PATH, paquet winget.
    static QString findLlamaServer(const QString& configuredPath = QString());

private:
    static void probeCpu(AIHardwareProfile& p);
    static void probeMemory(AIHardwareProfile& p);
    static void probeRuntimes(AIHardwareProfile& p);
};

} // namespace TSA::AI
