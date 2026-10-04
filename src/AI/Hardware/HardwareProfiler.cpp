// Fichier exclu du PCH (CMakeLists) : il inclut <windows.h> et <intrin.h> avec ses propres réglages.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#endif

#include "HardwareProfiler.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <cstring>

namespace TSA::AI
{

const GpuDevice* AIHardwareProfile::bestDiscreteGpu() const
{
    const GpuDevice* best = nullptr;
    for (const auto& g : gpus)
    {
        if (g.integrated) continue;
        if (!best || g.freeGB > best->freeGB) best = &g;
    }
    return best;
}

QJsonObject AIHardwareProfile::toJson() const
{
    QJsonArray gpuArray;
    for (const auto& g : gpus)
    {
        gpuArray.append(QJsonObject{
            { "deviceId", g.deviceId }, { "backend", g.backend }, { "name", g.name }, { "vendor", g.vendor },
            { "totalGB", g.totalGB }, { "freeGB", g.freeGB }, { "integrated", g.integrated } });
    }
    return QJsonObject{
        { "cpuName", cpuName }, { "physicalCores", physicalCores }, { "logicalCores", logicalCores },
        { "avx", avx }, { "avx2", avx2 }, { "fma", fma }, { "avx512", avx512 }, { "cpuScore", cpuScore },
        { "ramGB", ramGB }, { "ramAvailableGB", ramAvailableGB }, { "gpus", gpuArray },
        { "cudaDriver", cudaDriver }, { "vulkanLoader", vulkanLoader }, { "hipRuntime", hipRuntime },
        { "metal", metal }, { "llamaServerPath", llamaServerPath }, { "llamaVersion", llamaVersion },
        { "llamaBackends", QJsonArray::fromStringList(llamaBackends) }, { "probeError", probeError } };
}

// -----------------------------------------------------------------------------
// Fonctions pures
// -----------------------------------------------------------------------------

std::vector<GpuDevice> HardwareProfiler::parseListDevices(const QString& output)
{
    // Ligne type : « Vulkan1: NVIDIA GeForce RTX 3050 Laptop GPU (3964 MiB, 3369 MiB free) »
    static const QRegularExpression re(
        QStringLiteral(R"(^\s*([A-Za-z]+)(\d+)\s*:\s*(.+?)\s*\(\s*(\d+)\s*MiB\s*,\s*(\d+)\s*MiB\s+free\s*\)\s*$)"));
    std::vector<GpuDevice> devices;
    for (const QString& line : output.split('\n'))
    {
        const auto m = re.match(line);
        if (!m.hasMatch()) continue;
        GpuDevice d;
        d.backend = m.captured(1);
        d.deviceId = m.captured(1) + m.captured(2);
        d.name = m.captured(3).trimmed();
        d.totalGB = m.captured(4).toDouble() / 1024.0;
        d.freeGB = m.captured(5).toDouble() / 1024.0;
        d.vendor = vendorFromName(d.name);
        d.integrated = looksIntegrated(d.vendor, d.name);
        devices.push_back(d);
    }
    return devices;
}

QStringList HardwareProfiler::backendsFromLibraries(const QStringList& fileNames)
{
    QStringList backends;
    auto add = [&](const QString& b) { if (!backends.contains(b)) backends << b; };
    for (const QString& f : fileNames)
    {
        const QString n = f.toLower();
        if (n.startsWith("ggml-cuda")) add("CUDA");
        else if (n.startsWith("ggml-vulkan")) add("Vulkan");
        else if (n.startsWith("ggml-hip")) add("HIP");
        else if (n.startsWith("ggml-sycl")) add("SYCL");
        else if (n.startsWith("ggml-metal")) add("Metal");
        else if (n.startsWith("ggml-cpu")) add("CPU");
    }
    return backends;
}

QString HardwareProfiler::vendorFromName(const QString& gpuName)
{
    const QString n = gpuName.toLower();
    if (n.contains("nvidia") || n.contains("geforce") || n.contains("quadro") || n.contains("rtx") || n.contains("tesla"))
        return "NVIDIA";
    if (n.contains("amd") || n.contains("radeon")) return "AMD";
    if (n.contains("intel") || n.contains("arc ") || n.contains("iris") || n.contains("uhd")) return "Intel";
    if (n.contains("apple")) return "Apple";
    return "Unknown";
}

bool HardwareProfiler::looksIntegrated(const QString& vendor, const QString& gpuName)
{
    const QString n = gpuName.toLower();
    if (vendor == "Intel") return !n.contains("arc"); // UHD / Iris (Xe) intégrés ; Arc = dédié
    if (vendor == "AMD")
        return n.contains("radeon(tm) graphics") || n.contains("radeon graphics") || n.contains("vega 8")
            || n.contains("vega 6") || n.contains("780m") || n.contains("760m") || n.contains("680m");
    if (vendor == "Apple") return false; // mémoire unifiée : traitée comme un GPU à part entière
    return false;
}

double HardwareProfiler::computeCpuScore(int physicalCores, bool avx2, bool avx512)
{
    // Indice relatif : l'inférence CPU est surtout limitée par la bande passante mémoire,
    // les cœurs au-delà de 8 apportent peu. 4 cœurs AVX2 ≈ 1.0.
    const double cores = std::min(std::max(physicalCores, 1), 8) / 4.0;
    double simd = 0.45; // SSE seulement
    if (avx2) simd = 1.0;
    if (avx512) simd = 1.15;
    return cores * simd;
}

QString HardwareProfiler::findLlamaServer(const QString& configuredPath)
{
#ifdef _WIN32
    const QString exe = QStringLiteral("llama-server.exe");
#else
    const QString exe = QStringLiteral("llama-server");
#endif
    if (!configuredPath.isEmpty() && QFileInfo(configuredPath).isExecutable()) return configuredPath;

    // Distribution embarquée avec TSA (prioritaire : version testée).
    const QString bundled = QCoreApplication::applicationDirPath() + "/ai/llama/" + exe;
    if (QFileInfo(bundled).isExecutable()) return bundled;

    const QString onPath = QStandardPaths::findExecutable("llama-server");
    if (!onPath.isEmpty()) return onPath;

#ifdef _WIN32
    // Paquet winget « ggml.llamacpp » (installé sans être forcément dans le PATH du processus).
    const QString wingetRoot = QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA")) + "/Microsoft/WinGet/Packages";
    QDir root(wingetRoot);
    for (const QString& d : root.entryList({ "ggml.llamacpp*" }, QDir::Dirs | QDir::NoDotAndDotDot))
    {
        const QString candidate = root.filePath(d) + "/" + exe;
        if (QFileInfo(candidate).isExecutable()) return candidate;
    }
#endif
    return QString();
}

// -----------------------------------------------------------------------------
// Détection
// -----------------------------------------------------------------------------

void HardwareProfiler::probeCpu(AIHardwareProfile& p)
{
    p.logicalCores = QThread::idealThreadCount();
    p.physicalCores = p.logicalCores;
#ifdef _WIN32
    int info[4] = { 0 };
    __cpuid(info, 0x80000000);
    const unsigned maxExt = static_cast<unsigned>(info[0]);
    if (maxExt >= 0x80000004)
    {
        char brand[49] = { 0 };
        for (int i = 0; i < 3; ++i)
        {
            __cpuid(info, 0x80000002 + i);
            std::memcpy(brand + 16 * i, info, 16);
        }
        p.cpuName = QString::fromLatin1(brand).simplified();
    }

    __cpuid(info, 0);
    const int maxLeaf = info[0];
    __cpuid(info, 1);
    const bool osxsave = (info[2] & (1 << 27)) != 0;
    p.sse42 = (info[2] & (1 << 20)) != 0;
    p.fma = (info[2] & (1 << 12)) != 0;
    const bool avxHw = (info[2] & (1 << 28)) != 0;
    unsigned long long xcr0 = 0;
    if (osxsave) xcr0 = _xgetbv(0);
    const bool osAvx = (xcr0 & 0x6) == 0x6;            // XMM + YMM sauvegardés par l'OS
    const bool osAvx512 = (xcr0 & 0xE6) == 0xE6;       // + opmask, ZMM
    p.avx = avxHw && osAvx;
    if (maxLeaf >= 7)
    {
        __cpuidex(info, 7, 0);
        p.avx2 = p.avx && (info[1] & (1 << 5)) != 0;
        p.avx512 = osAvx512 && (info[1] & (1 << 16)) != 0; // AVX-512F
    }

    // Cœurs physiques
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    if (len > 0)
    {
        std::vector<char> buffer(len);
        auto* base = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, base, &len))
        {
            int cores = 0;
            for (DWORD off = 0; off < len;)
            {
                auto* item = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + off);
                if (item->Relationship == RelationProcessorCore) ++cores;
                off += item->Size;
            }
            if (cores > 0) p.physicalCores = cores;
        }
    }
#endif
    if (p.cpuName.isEmpty()) p.cpuName = QSysInfo::currentCpuArchitecture();
    p.cpuScore = computeCpuScore(p.physicalCores, p.avx2, p.avx512);
}

void HardwareProfiler::probeMemory(AIHardwareProfile& p)
{
#ifdef _WIN32
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st))
    {
        p.ramGB = static_cast<double>(st.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        p.ramAvailableGB = static_cast<double>(st.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
    }
#endif
}

void HardwareProfiler::probeRuntimes(AIHardwareProfile& p)
{
#ifdef _WIN32
    wchar_t sys[MAX_PATH] = { 0 };
    GetSystemDirectoryW(sys, MAX_PATH);
    const QDir sysDir(QString::fromWCharArray(sys));
    p.cudaDriver = sysDir.exists("nvcuda.dll");
    p.vulkanLoader = sysDir.exists("vulkan-1.dll");
    p.hipRuntime = !sysDir.entryList({ "amdhip64*.dll" }, QDir::Files).isEmpty();
#elif defined(__APPLE__)
    p.metal = true;
#endif
}

AIHardwareProfile HardwareProfiler::probe(const QString& llamaServerPath, int timeoutMs)
{
    AIHardwareProfile p;
    probeCpu(p);
    probeMemory(p);
    probeRuntimes(p);

    p.llamaServerPath = llamaServerPath;
    if (llamaServerPath.isEmpty())
    {
        p.probeError = QStringLiteral("Moteur d'inférence local (llama-server) introuvable.");
        return p;
    }

    const QFileInfo exeInfo(llamaServerPath);
    p.llamaBackends = backendsFromLibraries(exeInfo.dir().entryList({ "ggml-*.dll", "libggml-*.so", "libggml-*.dylib" }, QDir::Files));

    auto run = [&](const QStringList& args, QString& out) {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(llamaServerPath, args);
        if (!proc.waitForFinished(timeoutMs))
        {
            proc.kill();
            proc.waitForFinished(2000);
            return false;
        }
        out = QString::fromLocal8Bit(proc.readAll());
        return true;
    };

    QString out;
    if (run({ "--version" }, out))
    {
        static const QRegularExpression ver(QStringLiteral(R"(version:\s*([^\r\n]+))"));
        const auto m = ver.match(out);
        if (m.hasMatch()) p.llamaVersion = m.captured(1).trimmed();
    }
    if (run({ "--list-devices" }, out))
        p.gpus = parseListDevices(out);
    else
        p.probeError = QStringLiteral("llama-server --list-devices n'a pas répondu à temps.");
    return p;
}

} // namespace TSA::AI
