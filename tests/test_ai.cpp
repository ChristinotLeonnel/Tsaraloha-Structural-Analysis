// Suite « ai » : IA de co-ingénierie (tests 112-123). Aucune inférence réelle : uniquement la
// logique déterministe (matériel, registre, sélection, vérification, contexte, outils, protocole).

#include "test_common.h"

#include "AI/Checker/StructuralChecker.h"
#include "AI/Context/EngineeringContext.h"
#include "AI/Core/AISettings.h"
#include "AI/Hardware/HardwareProfiler.h"
#include "AI/Models/ModelRegistry.h"
#include "AI/Providers/LocalLlamaServer.h"
#include "AI/Providers/OpenAICompatibleProvider.h"
#include "AI/RAG/EngineeringKnowledgeBase.h"
#include "AI/Tools/AIToolRegistry.h"
#include "Model/Load/LoadCase.h"
#include "Model/Load/NodalLoad.h"
#include "Model/Model.h"

#include <QJsonDocument>

using namespace TSA::AI;
using TSA::Model::Material;
using TSA::Model::Model;
using TSA::Model::Section;
using TSA::Model::SupportDefinition;

namespace
{

// Sortie réelle de « llama-server --list-devices » (build Vulkan, portable RTX 3050 + Intel UHD).
const char* kListDevicesSample =
    "0.00.000.647 I srv  llama_server: initializing ...\n"
    "Available devices:\n"
    "  Vulkan0: Intel(R) UHD Graphics (8201 MiB, 7413 MiB free)\n"
    "  Vulkan1: NVIDIA GeForce RTX 3050 Laptop GPU (3964 MiB, 3369 MiB free)\n";

AIHardwareProfile laptopProfile()
{
    AIHardwareProfile p;
    p.cpuName = "i5-11400H";
    p.physicalCores = 6;
    p.logicalCores = 12;
    p.avx = p.avx2 = p.fma = true;
    p.avx512 = true;
    p.cpuScore = HardwareProfiler::computeCpuScore(6, true, true);
    p.ramGB = 15.8;
    p.ramAvailableGB = 9.0;
    p.gpus = HardwareProfiler::parseListDevices(kListDevicesSample);
    return p;
}

// Portique simple : 2 poteaux encastrés, 1 poutre, 1 cas de charge avec une charge nodale.
void makePortal(Model& m, int* beamId = nullptr)
{
    const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3), c = m.addNode(6, 0, 3), d = m.addNode(6, 0, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    m.getNode(d)->setSupport(SupportDefinition::fixed());
    m.addColumn(a, b, Section::heb(200), Material::steelS235());
    m.addColumn(d, c, Section::heb(200), Material::steelS235());
    const int beam = m.addBar(b, c, Section::ipe(300), Material::steelS235(), TSA::Model::BarRole::Beam);
    const int lc = m.loadManager().addLoadCase(TSA::Model::LoadCase(0, "G"));
    m.loadManager().addNodalLoad(TSA::Model::NodalLoad(0, b, lc, 10.0, 0.0, -20.0));
    if (beamId) *beamId = beam;
}

bool hasCode(const CheckReport& r, const QString& code)
{
    return std::any_of(r.findings.begin(), r.findings.end(), [&](const CheckFinding& f) { return f.code == code; });
}

} // namespace

bool runSuite_AI(int& passed)
{
    // TEST 112 : GPU vus par llama.cpp, iGPU exclu, backends
    {
        const auto devices = HardwareProfiler::parseListDevices(kListDevicesSample);
        TEST_CHECK(devices.size() == 2, "Test 112: 2 périphériques détectés");
        TEST_CHECK(devices[0].deviceId == "Vulkan0" && devices[0].vendor == "Intel" && devices[0].integrated, "Test 112: Intel UHD = iGPU");
        TEST_CHECK(devices[1].deviceId == "Vulkan1" && devices[1].vendor == "NVIDIA" && !devices[1].integrated, "Test 112: RTX = GPU dédié");
        TEST_CHECK(approxEqual(devices[1].freeGB, 3369.0 / 1024.0, 1e-9), "Test 112: VRAM libre en Go");
        TEST_CHECK(HardwareProfiler::parseListDevices("CUDA0: NVIDIA GeForce RTX 4090 (24564 MiB, 23000 MiB free)").front().backend == "CUDA",
                   "Test 112: backend CUDA reconnu");
        const auto backends = HardwareProfiler::backendsFromLibraries({ "ggml-base.dll", "ggml-cpu-haswell.dll", "ggml-vulkan.dll", "llama.dll" });
        TEST_CHECK(backends.contains("Vulkan") && backends.contains("CPU") && !backends.contains("CUDA"), "Test 112: backends du moteur");
        AIHardwareProfile p = laptopProfile();
        TEST_CHECK(p.bestDiscreteGpu() && p.bestDiscreteGpu()->deviceId == "Vulkan1", "Test 112: meilleur GPU dédié");
        std::cout << "[PASS] Test 112: Détection GPU (llama.cpp) et backends" << std::endl;
        ++passed;
    }

    // TEST 113 : registre embarqué (données sourcées, sha256 présents)
    {
        QString err;
        const ModelRegistry reg = ModelRegistry::loadDefault(&err);
        TEST_CHECK(reg.models().size() >= 7, "Test 113: registre chargé depuis :/ai/model_registry.json");
        const ModelSpec* q4 = reg.find("qwen3-4b-q4_k_m");
        TEST_CHECK(q4 && q4->fileSizeBytes == 2497280256LL && q4->sha256.size() == 64, "Test 113: Qwen3 4B Q4_K_M (taille, sha256)");
        TEST_CHECK(approxEqual(q4->kvCacheGB(8192), 147456.0 * 8192 / (1024.0 * 1024 * 1024)), "Test 113: cache KV");
        TEST_CHECK(q4->downloadUrl() == "https://huggingface.co/Qwen/Qwen3-4B-GGUF/resolve/main/Qwen3-4B-Q4_K_M.gguf", "Test 113: URL");
        TEST_CHECK(reg.findByFileName("qwen3-8b-q4_k_m.gguf") != nullptr, "Test 113: recherche par fichier (casse ignorée)");
        const ModelSpec* moe = reg.find("qwen3-30b-a3b-q4_k_m");
        TEST_CHECK(moe && moe->activeParametersB < moe->parametersB, "Test 113: MoE — paramètres actifs");
        std::cout << "[PASS] Test 113: Registre de modèles" << std::endl;
        ++passed;
    }

    // TEST 114 : sélection automatique selon le matériel
    {
        const ModelRegistry reg = ModelRegistry::loadDefault();
        // Portable RTX 3050 4 Go : 4B ne tient pas en VRAM avec son contexte → hybride sur le GPU dédié.
        const auto laptop = ModelSelector::recommend(laptopProfile(), reg);
        TEST_CHECK(laptop.model != nullptr, "Test 114: recommandation portable");
        TEST_CHECK(laptop.deviceId == "Vulkan1", "Test 114: GPU dédié choisi, jamais l'iGPU");
        TEST_CHECK(laptop.mode == ExecutionMode::LocalHybrid || laptop.mode == ExecutionMode::LocalGPU, "Test 114: exécution GPU/hybride");
        TEST_CHECK(laptop.estimatedTokensPerSec >= 8.0, "Test 114: vitesse interactive");
        TEST_CHECK(laptop.contextSize >= 8192, "Test 114: contexte complet (assistant à outils), jamais réduit sans nécessité");
        // Mémoire libre réellement mesurée sur ce poste (2.8 Go) : Q4 avec contexte complet plutôt que Q5 réduit.
        AIHardwareProfile busy = laptopProfile();
        busy.ramAvailableGB = 2.8;
        const auto b = ModelSelector::recommend(busy, reg);
        TEST_CHECK(b.model && b.contextSize >= 8192 && b.model->quantization == "Q4_K_M", "Test 114: contexte complet prioritaire sur la quantification");

        // PC étudiant : pas de GPU, 8 Go, 4 cœurs AVX2 → CPU, modèle léger.
        AIHardwareProfile student;
        student.physicalCores = 4;
        student.avx2 = true;
        student.cpuScore = HardwareProfiler::computeCpuScore(4, true, false);
        student.ramGB = 8.0;
        student.ramAvailableGB = 5.0;
        const auto s = ModelSelector::recommend(student, reg);
        TEST_CHECK(s.model && s.mode == ExecutionMode::LocalCPU && s.backend == "CPU", "Test 114: PC sans GPU → CPU");
        TEST_CHECK(s.model->fileSizeBytes < 3000000000LL, "Test 114: modèle léger sur CPU");

        // Station 24 Go de VRAM : modèle « powerful » entièrement sur GPU.
        AIHardwareProfile ws = student;
        ws.physicalCores = 16;
        ws.cpuScore = HardwareProfiler::computeCpuScore(16, true, true);
        ws.ramGB = 64.0;
        ws.ramAvailableGB = 48.0;
        ws.gpus = HardwareProfiler::parseListDevices("CUDA0: NVIDIA GeForce RTX 4090 (24564 MiB, 23000 MiB free)");
        const auto w = ModelSelector::recommend(ws, reg);
        TEST_CHECK(w.model && w.model->tier == "powerful" && w.mode == ExecutionMode::LocalGPU && w.deviceId == "CUDA0",
                   "Test 114: station GPU → modèle puissant sur CUDA");

        // Machine très faible : 4 Go → ressources limitées ou Cloud proposé, jamais un plantage.
        AIHardwareProfile weak;
        weak.physicalCores = 2;
        weak.cpuScore = HardwareProfiler::computeCpuScore(2, false, false);
        weak.ramGB = 4.0;
        weak.ramAvailableGB = 2.0;
        const auto k = ModelSelector::recommend(weak, reg);
        TEST_CHECK(k.limitedResources, "Test 114: ressources limitées signalées");
        TEST_CHECK(!k.model || k.model->fileSizeBytes < 1000000000LL, "Test 114: au plus le modèle compact");
        // Auto-benchmark réel de ce portable (Qwen3 0.6B) : CPU 35,2 j/s, RTX 3050 via Vulkan 4,2 j/s.
        // Les mesures priment : le GPU « détecté » n'est pas choisi s'il est mesuré plus lent.
        AIHardwareProfile measured = laptopProfile();
        const ModelSpec* tiny = reg.find("qwen3-0.6b-q8_0");
        measured.measuredBandwidthGBs["CPU"] = ModelSelector::bandwidthFromMeasurement(35.2, *tiny);
        measured.measuredBandwidthGBs["Vulkan1"] = ModelSelector::bandwidthFromMeasurement(4.2, *tiny);
        const auto mr = ModelSelector::recommend(measured, reg);
        TEST_CHECK(mr.model && mr.mode == ExecutionMode::LocalCPU && mr.deviceId.isEmpty(), "Test 114: GPU mesuré lent → CPU");
        if (auto e = ModelSelector::evaluate(measured, *tiny))
            TEST_CHECK(std::abs(e->estimatedTokensPerSec - 35.2) < 0.5, "Test 114: la mesure est restituée pour le modèle mesuré");
        std::cout << "[PASS] Test 114: Sélection automatique (portable, étudiant, station, machine faible, mesures)" << std::endl;
        ++passed;
    }

    // TEST 115 : StructuralChecker — modèle sain
    {
        Model m;
        makePortal(m);
        const CheckReport r = StructuralChecker::check(m);
        TEST_CHECK(r.errors() == 0, "Test 115: portique encastré sans erreur");
        TEST_CHECK(!hasCode(r, "STAB_NO_SUPPORT") && !hasCode(r, "TOPO_ISOLATED_NODE"), "Test 115: pas de faux positif");
        std::cout << "[PASS] Test 115: Vérification — modèle sain" << std::endl;
        ++passed;
    }

    // TEST 116 : StructuralChecker — défauts réels
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0), c = m.addNode(10, 0, 0);
        m.addNode(20, 20, 20); // isolé
        const int b1 = m.addBar(a, b, Section::ipe(200), Material::steelS235(), TSA::Model::BarRole::Beam);
        const int b2 = m.addBar(a, b, Section::ipe(200), Material::steelS235(), TSA::Model::BarRole::Beam); // superposée
        m.addBar(b, c, Section::ipe(200), Material::steelS235(), TSA::Model::BarRole::Beam);
        m.loadManager().addLoadCase(TSA::Model::LoadCase(0, "Q vide", TSA::Model::LoadCaseCategory::Live));
        CheckReport r = StructuralChecker::check(m);
        TEST_CHECK(hasCode(r, "STAB_NO_SUPPORT"), "Test 116: aucun appui → mécanisme");
        TEST_CHECK(hasCode(r, "TOPO_ISOLATED_NODE"), "Test 116: nœud isolé");
        TEST_CHECK(hasCode(r, "TOPO_DUPLICATE_MEMBER"), "Test 116: barres superposées");
        TEST_CHECK(hasCode(r, "LOAD_EMPTY_CASE"), "Test 116: cas de charge vide");
        TEST_CHECK(r.findings.front().severity == FindingSeverity::Error, "Test 116: erreurs en tête");
        (void)b1; (void)b2;

        // Appui ponctuel articulé (translations seules) sur une sous-structure : rotation d'ensemble.
        m.getNode(a)->setSupport(SupportDefinition::pinned());
        r = StructuralChecker::check(m);
        TEST_CHECK(!hasCode(r, "STAB_NO_SUPPORT") && hasCode(r, "STAB_SINGLE_PIN"), "Test 116: appui articulé unique");
        std::cout << "[PASS] Test 116: Vérification — défauts détectés" << std::endl;
        ++passed;
    }

    // TEST 117 : contexte d'ingénierie (données réelles, unités)
    {
        int beam = 0;
        Model m;
        makePortal(m, &beam);
        EngineeringSources src;
        src.model = &m;
        src.projectName = "Portique test";
        src.selection = { { "beam", beam } };
        const QJsonObject ctx = EngineeringContextBuilder::summary(src);
        TEST_CHECK(ctx["project"].toObject()["counts"].toObject()["columns"].toInt() == 2, "Test 117: effectifs");
        TEST_CHECK(ctx["units"].toObject()["force"].toString() == "kN", "Test 117: unités explicites");
        TEST_CHECK(ctx["sections"].toArray().size() == 2, "Test 117: sections distinctes (HEB 200, IPE 300)");
        TEST_CHECK(!ctx["results"].toObject()["available"].toBool(), "Test 117: résultats absents déclarés");
        const QJsonObject det = EngineeringContextBuilder::objectDetails(src, { "poutre", beam });
        TEST_CHECK(approxEqual(det["length_m"].toDouble(), 6.0), "Test 117: longueur réelle de la poutre");
        TEST_CHECK(det["material"].toObject()["E_GPa"].toDouble() > 150.0, "Test 117: E en GPa");
        TEST_CHECK(ctx.contains("selection"), "Test 117: sélection incluse");
        TEST_CHECK(EngineeringContextBuilder::objectDetails(src, { "beam", 999 }).isEmpty(), "Test 117: objet inexistant → vide");
        std::cout << "[PASS] Test 117: Contexte d'ingénierie" << std::endl;
        ++passed;
    }

    // TEST 118 : outils — liste blanche et lecture seule
    {
        Model m;
        makePortal(m);
        AIToolRegistry tools([&] { EngineeringSources s; s.model = &m; return s; }, nullptr);
        TEST_CHECK(!tools.execute("run_shell", "{\"cmd\":\"del *\"}").ok, "Test 118: outil hors liste refusé");
        TEST_CHECK(!tools.execute("get_object", "pas du json").ok, "Test 118: arguments invalides refusés");
        const auto obj = tools.execute("get_object", "{\"type\":\"column\",\"id\":\"C1\"}");
        TEST_CHECK(obj.ok && obj.data["type"].toString() == "column", "Test 118: get_object");
        const auto chk = tools.execute("check_model", "{}");
        TEST_CHECK(chk.ok && chk.data["errors"].toInt() == 0, "Test 118: check_model");
        const auto list = tools.execute("list_members", "{\"type\":\"beam\",\"limit\":500}");
        TEST_CHECK(list.ok && list.data["members"].toArray().size() == 1, "Test 118: list_members filtré");
        TEST_CHECK(tools.toolDefinitions().size() == AIToolRegistry::whitelist().size(), "Test 118: définitions = liste blanche");
        std::cout << "[PASS] Test 118: Outils — liste blanche" << std::endl;
        ++passed;
    }

    // TEST 119 : proposition (human-in-the-loop) : rien ne change avant acceptation, Undo ensuite
    {
        int beam = 0;
        Model m;
        makePortal(m, &beam);
        AIToolRegistry tools([&] { EngineeringSources s; s.model = &m; return s; }, nullptr);
        const auto out = tools.execute("propose_section_change",
                                       QString("{\"type\":\"beam\",\"ids\":[%1],\"section\":\"IPE 360\",\"rationale\":\"test\"}").arg(beam));
        TEST_CHECK(out.ok && out.proposal.has_value(), "Test 119: proposition créée");
        TEST_CHECK(m.getBeam(beam)->section().name.find("300") != std::string::npos, "Test 119: modèle inchangé avant acceptation");
        TEST_CHECK(!tools.execute("propose_section_change", "{\"type\":\"beam\",\"ids\":[1],\"section\":\"IPE 310\",\"rationale\":\"x\"}").ok,
                   "Test 119: profil inexistant refusé");
        QString err;
        TEST_CHECK(AIToolRegistry::applyProposal(*out.proposal, m, &err), "Test 119: application acceptée");
        TEST_CHECK(m.getBeam(beam)->section().name.find("360") != std::string::npos, "Test 119: section appliquée");
        TEST_CHECK(m.canUndo() && m.undo(), "Test 119: annulable");
        TEST_CHECK(m.getBeam(beam)->section().name.find("300") != std::string::npos, "Test 119: Undo restaure IPE 300");
        TEST_CHECK(AIToolRegistry::parseSection("rect 300x500").has_value() && !AIToolRegistry::parseSection("XYZ").has_value(),
                   "Test 119: analyse des profils");
        std::cout << "[PASS] Test 119: Propositions validées par l'ingénieur + Undo" << std::endl;
        ++passed;
    }

    // TEST 120 : protocole Chat Completions (SSE, appels d'outils, corps de requête)
    {
        OpenAICompatibleProvider::StreamState st;
        QString content;
        OpenAICompatibleProvider::consumeSse(st, "data: {\"choices\":[{\"delta\":{\"content\":\"Bon\"}}]}\n\ndata: {\"choices\":[{\"delta\":{\"content\":\"jour\"}}]}\n", &content, nullptr);
        OpenAICompatibleProvider::consumeSse(st,
            "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"c1\",\"function\":{\"name\":\"get_object\",\"arguments\":\"{\\\"type\\\":\"}}]}}]}\n"
            "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\\\"beam\\\",\\\"id\\\":3}\"}}]},\"finish_reason\":\"tool_calls\"}]}\n"
            "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":120,\"completion_tokens\":7},\"timings\":{\"predicted_per_second\":21.5}}\n"
            "data: [DONE]\n", &content, nullptr);
        OpenAICompatibleProvider::finalizeToolCalls(st);
        TEST_CHECK(content == "Bonjour" && st.result.content == "Bonjour", "Test 120: texte en flux");
        TEST_CHECK(st.result.toolCalls.size() == 1 && st.result.toolCalls[0].name == "get_object", "Test 120: appel d'outil reconstitué");
        TEST_CHECK(QJsonDocument::fromJson(st.result.toolCalls[0].argumentsJson.toUtf8()).object()["id"].toInt() == 3, "Test 120: arguments fragmentés");
        TEST_CHECK(st.done && st.result.completionTokens == 7 && approxEqual(st.result.tokensPerSecond(), 21.5), "Test 120: usage et vitesse moteur");

        OpenAICompatibleConfig local;
        local.llamaCppExtensions = true;
        OpenAICompatibleConfig cloud;
        cloud.locality = ProviderLocality::Cloud;
        ChatRequest req;
        req.messages = { ChatMessage::user("x") };
        TEST_CHECK(OpenAICompatibleProvider(local).buildRequestBody(req).contains("chat_template_kwargs"), "Test 120: extension llama.cpp en local");
        TEST_CHECK(!OpenAICompatibleProvider(cloud).buildRequestBody(req).contains("chat_template_kwargs"), "Test 120: corps standard pour le Cloud");
        std::cout << "[PASS] Test 120: Protocole OpenAI-compatible (SSE + outils)" << std::endl;
        ++passed;
    }

    // TEST 121 : ligne de commande du moteur local
    {
        LlamaServerConfig c;
        c.modelPath = "m.gguf";
        c.contextSize = 4096;
        QStringList cpu = LocalLlamaServer::buildArguments(c, 8123);
        TEST_CHECK(cpu.contains("--device") && cpu[cpu.indexOf("--device") + 1] == "none" && cpu.contains("-ngl"), "Test 121: CPU seul");
        TEST_CHECK(cpu[cpu.indexOf("--host") + 1] == "127.0.0.1", "Test 121: écoute locale uniquement");
        c.deviceId = "Vulkan1";
        QStringList gpu = LocalLlamaServer::buildArguments(c, 8123);
        TEST_CHECK(gpu[gpu.indexOf("--device") + 1] == "Vulkan1" && gpu.contains("--fit"), "Test 121: GPU + répartition auto (hybride)");
        c.gpuLayers = 20;
        QStringList forced = LocalLlamaServer::buildArguments(c, 8123);
        TEST_CHECK(forced[forced.indexOf("-ngl") + 1] == "20" && !forced.contains("--fit"), "Test 121: couches imposées");
        std::cout << "[PASS] Test 121: Arguments llama-server" << std::endl;
        ++passed;
    }

    // TEST 122 : RAG local — sources citées, pas d'invention
    {
        EngineeringKnowledgeBase kb;
        kb.addDocument("docs/ANALYSIS.md", "# Analyse\nTSA transmet à OpenSees les barres, treillis, câbles et appuis.\n\n# Flèches\nLe critère de flèche du projet est défini par l'ingénieur, par exemple L/300 pour les planchers.");
        kb.addDocument("docs/UI.md", "# Ruban\nLe ruban contient les onglets Accueil, Modèle et Résultats de l'application TSA.");
        auto hits = kb.search("critère de flèche plancher");
        TEST_CHECK(!hits.empty() && hits.front().chunk->heading == "Flèches" && hits.front().chunk->source == "docs/ANALYSIS.md", "Test 122: extrait pertinent et sourcé");
        TEST_CHECK(kb.search("béton précontraint fluage").empty(), "Test 122: aucune source → aucun extrait");
        TEST_CHECK(EngineeringKnowledgeBase::tokenize("Flèche ÉLEVÉE").contains("fleche"), "Test 122: normalisation des accents");
        std::cout << "[PASS] Test 122: Base de connaissances (BM25)" << std::endl;
        ++passed;
    }

    // TEST 123 : stockage protégé de la clé API
    {
        const QByteArray secret = "sk-test-0123456789";
        const QByteArray cipher = SecretStore::protect(secret);
        TEST_CHECK(!cipher.isEmpty(), "Test 123: chiffrement");
        if (SecretStore::isProtected()) TEST_CHECK(!cipher.contains(secret), "Test 123: clé non stockée en clair");
        TEST_CHECK(SecretStore::unprotect(cipher) == secret, "Test 123: déchiffrement");
        TEST_CHECK(aiModeFromKey(aiModeKey(AIMode::Auto)) == AIMode::Auto && aiModeFromKey("??") == AIMode::Local, "Test 123: mode par défaut LOCAL");
        std::cout << "[PASS] Test 123: Secret API (DPAPI) et réglages" << std::endl;
        ++passed;
    }
    return true;
}
