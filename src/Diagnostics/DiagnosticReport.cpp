#include "DiagnosticReport.h"
#include "Logger.h"
#include "../Model/Model.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <QSysInfo>
#include <QCoreApplication>

namespace fs = std::filesystem;

namespace TSA::Diagnostics
{

std::string DiagnosticReport::exportReport(const TSA::Model::Model* model, const std::string& targetDirectory)
{
    std::string outDir = targetDirectory;
    if (outDir.empty())
    {
        outDir = Logger::instance().logsDirectory();
    }
    if (outDir.empty())
    {
        outDir = "logs";
    }

    try {
        fs::create_directories(outDir);
    } catch (...) {}

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuffer{};
#ifdef _WIN32
    localtime_s(&tmBuffer, &t);
#else
    localtime_r(&t, &tmBuffer);
#endif
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d_%H-%M-%S", &tmBuffer);

    std::string reportPath = outDir + "/TSA_Diagnostic_Report_" + timeStr + ".txt";
    std::ofstream out(reportPath, std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        return "";
    }

    char displayTime[64];
    std::strftime(displayTime, sizeof(displayTime), "%Y-%m-%d %H:%M:%S", &tmBuffer);

    out << "================================================================================\n";
    out << "TSA DIAGNOSTIC REPORT (RAPPORT DE DIAGNOSTIC TECHNIQUE)\n";
    out << "Tsaraloha Structural Analysis\n";
    out << "================================================================================\n\n";

    // 1. Informations système & application
    out << "--- 1. INFORMATIONS SYSTÈME & ENVIRONNEMENT ---\n";
    out << "Rapport généré le : " << displayTime << "\n";
    out << "Application       : TSA (Tsaraloha Structural Analysis)\n";
    out << "Version           : 0.1.0\n";
    out << "Session ID        : " << Logger::instance().sessionId() << "\n";
    out << "Fichier Log       : " << Logger::instance().sessionLogPath() << "\n";
    out << "Système d'exploit.: " << QSysInfo::prettyProductName().toStdString() << "\n";
    out << "Architecture CPU  : " << QSysInfo::currentCpuArchitecture().toStdString() << "\n";
    out << "Version Qt        : " << qVersion() << "\n";
    out << "Version OCCT      : 8.0.1\n";
#if defined(_DEBUG)
    out << "Type de build     : Debug\n";
#else
    out << "Type de build     : Release (Optimisé)\n";
#endif
    out << "\n";

    // 2. État du modèle structural
    out << "--- 2. ÉTAT DU MODÈLE STRUCTURAL ---\n";
    if (model)
    {
        out << "Nœuds             : " << model->nodes().size() << "\n";
        out << "Poutres           : " << model->beams().size() << "\n";
        out << "Poteaux           : " << model->columns().size() << "\n";
        out << "Dalles / Planchers: " << model->slabs().size() << "\n";
        out << "Voiles / Murs     : " << model->walls().size() << "\n";
        out << "Semelles / Fondat.: " << model->foundations().size() << "\n";
        out << "Treillis          : " << model->trussMembers().size() << "\n";
        out << "Câbles structuraux: " << model->cables().size() << "\n";
        out << "Document modifié  : " << (model->isModified() ? "OUI (Dirty)" : "NON (Sauvegardé)") << "\n";
        out << "Historique Undo   : " << (model->canUndo() ? model->lastUndoActionName() : "Vide") << "\n";
    }
    else
    {
        out << "Aucun modèle structural actif n'a été spécifié pour ce rapport.\n";
    }
    out << "\n";

    // 3. Dernière commande exécutée
    CommandInfo lastCmd = Logger::instance().lastCommand();
    out << "--- 3. DERNIÈRE COMMANDE EXÉCUTÉE ---\n";
    if (!lastCmd.name.empty())
    {
        out << "Nom de la commande : " << lastCmd.name << "\n";
        out << "Identifiant (ID)   : " << lastCmd.id << "\n";
        out << "Paramètres         : " << lastCmd.parameters << "\n";
        out << "Heure de début     : " << lastCmd.startTime << "\n";
    }
    else
    {
        out << "Aucune commande enregistrée lors de la génération du rapport.\n";
    }
    out << "\n";

    // 4. Les 100 derniers événements en mémoire
    out << "--- 4. LES 100 DERNIERS ÉVÉNEMENTS (SÉQUENCE CHRONOLOGIQUE) ---\n";
    auto events = Logger::instance().recentEvents();
    if (events.empty())
    {
        out << "Aucun événement dans le tampon circulaire.\n";
    }
    else
    {
        for (size_t i = 0; i < events.size(); ++i)
        {
            out << "[" << std::setw(3) << (i + 1) << "] " << events[i].formatForLog() << "\n";
        }
    }
    out << "\n";

    out << "================================================================================\n";
    out << "FIN DU RAPPORT DE DIAGNOSTIC\n";
    out << "================================================================================\n";
    out.flush();
    out.close();

    TSA_LOG_INFO("DiagnosticReport", "ReportExported", "Rapport exporté : " + reportPath);

    return reportPath;
}

} // namespace TSA::Diagnostics
