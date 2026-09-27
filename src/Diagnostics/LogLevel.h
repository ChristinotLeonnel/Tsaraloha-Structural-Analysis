#pragma once

#include <string>

namespace TSA::Diagnostics
{

/**
 * @brief Niveaux de gravité des messages de diagnostic TSA.
 */
enum class LogLevel
{
    Trace,     // Diagnostic ultra-fin (mode développeur)
    Debug,     // Diagnostic technique (mode développeur)
    Info,      // Événement opérationnel standard
    Warning,   // Anomalie non bloquante
    Error,     // Erreur fonctionnelle ou d'opération
    Critical   // Erreur critique ou crash imminent
};

inline const char* logLevelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Trace:    return "TRACE";
    case LogLevel::Debug:    return "DEBUG";
    case LogLevel::Info:     return "INFO";
    case LogLevel::Warning:  return "WARN";
    case LogLevel::Error:    return "ERROR";
    case LogLevel::Critical: return "CRIT";
    }
    return "UNKNOWN";
}

inline LogLevel stringToLogLevel(const std::string& str)
{
    if (str == "TRACE") return LogLevel::Trace;
    if (str == "DEBUG") return LogLevel::Debug;
    if (str == "WARN" || str == "WARNING") return LogLevel::Warning;
    if (str == "ERROR" || str == "ERR") return LogLevel::Error;
    if (str == "CRIT" || str == "CRITICAL") return LogLevel::Critical;
    return LogLevel::Info;
}

} // namespace TSA::Diagnostics
