#pragma once

#include "LogLevel.h"
#include <string>
#include <sstream>
#include <iomanip>

namespace TSA::Diagnostics
{

/**
 * @brief Structure d'un événement de journalisation horodaté et contextualisé.
 */
struct LogEntry
{
    std::string timestamp;    // Format : YYYY-MM-DD HH:MM:SS.mmm
    LogLevel level = LogLevel::Info;
    std::string module;       // Ex: "CartesianGrid", "Command", "Model", "OCCT", "UI"
    std::string eventName;    // Ex: "CartesianGridYChanged", "CommandStarted"
    std::string message;      // Message descriptif ou clé=valeur
    std::string file;         // Fichier source
    int line = 0;             // Ligne source
    std::string function;     // Fonction appelante
    uint64_t sequenceId = 0;  // Compteur séquentiel d'événement

    std::string formatForLog() const
    {
        std::ostringstream oss;
        oss << "[" << timestamp << "] "
            << "[" << std::left << std::setw(5) << logLevelToString(level) << "] "
            << "[" << std::setw(14) << (module.empty() ? "General" : module) << "] ";
        if (!eventName.empty())
        {
            oss << "[" << eventName << "] ";
        }
        oss << message;
        if (!file.empty() && line > 0)
        {
            // Conserver uniquement le nom de base du fichier pour la lisibilité
            size_t pos = file.find_last_of("/\\");
            std::string baseFile = (pos != std::string::npos) ? file.substr(pos + 1) : file;
            oss << " (" << baseFile << ":" << line << ")";
        }
        return oss.str();
    }

    std::string formatCompact() const
    {
        std::ostringstream oss;
        oss << "[" << timestamp.substr(11, 8) << "] "
            << "[" << logLevelToString(level) << "] ";
        if (!eventName.empty())
        {
            oss << eventName << " : ";
        }
        oss << message;
        return oss.str();
    }

    std::string format() const
    {
        return formatForLog();
    }

    bool operator==(const LogEntry& other) const
    {
        return sequenceId == other.sequenceId &&
               level == other.level &&
               module == other.module &&
               eventName == other.eventName &&
               message == other.message;
    }
};

} // namespace TSA::Diagnostics
