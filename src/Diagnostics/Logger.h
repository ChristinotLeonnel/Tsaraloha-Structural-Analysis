#pragma once

#include "LogLevel.h"
#include "LogEntry.h"
#include "RingBuffer.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <fstream>
#include <mutex>
#include <atomic>
#include <map>

namespace TSA::Diagnostics
{

struct CommandInfo
{
    std::string id;
    std::string name;
    std::string parameters;
    std::string startTime;

    bool operator==(const std::string& other) const { return name == other || id == other; }
    bool operator==(const char* other) const { return other && (name == other || id == other); }
    bool operator==(const CommandInfo& other) const { return id == other.id && name == other.name; }
};

/**
 * @brief Gestionnaire central de journalisation et de diagnostic pour TSA.
 * Thread-safe, haute performance, gestion de session et rotation de logs.
 */
class Logger
{
public:
    using LogListener = std::function<void(const LogEntry&)>;

    static Logger& instance();

    /**
     * @brief Initialise le système de logs et démarre une nouvelle session.
     * @param baseDir Dossier racine pour les logs (par défaut dossier "logs" sous l'exécutable ou le projet).
     */
    bool init(const std::string& baseDir = "");

    /**
     * @brief Ferme proprement la session de log et vide les tampons.
     */
    void shutdown();

    // Mode développeur (active les logs Debug et Trace)
    void setDeveloperMode(bool enabled);
    bool isDeveloperMode() const;
    void setDeveloperModeEnabled(bool enabled) { setDeveloperMode(enabled); }
    bool isDeveloperModeEnabled() const { return isDeveloperMode(); }

    // Métadonnées de session
    const std::string& sessionId() const { return m_sessionId; }
    const std::string& sessionLogPath() const { return m_sessionLogPath; }
    const std::string& logsDirectory() const { return m_logsDirectory; }

    // Suivi de la dernière commande exécutée
    void setLastCommand(const std::string& id, const std::string& name, const std::string& parameters = "");
    void setLastCommand(const std::string& name, const std::string& parameters = "")
    {
        setLastCommand(name, name, parameters);
    }
    void clearLastCommand();
    CommandInfo lastCommand() const;

    // Journalisation générale
    void log(LogLevel level,
             const std::string& module,
             const std::string& eventName,
             const std::string& message,
             const char* file = nullptr,
             int line = 0,
             const char* function = nullptr);

    // Helpers d'écriture
    void trace(const std::string& module, const std::string& event, const std::string& msg, const char* f = nullptr, int l = 0, const char* fn = nullptr);
    void debug(const std::string& module, const std::string& event, const std::string& msg, const char* f = nullptr, int l = 0, const char* fn = nullptr);
    void info(const std::string& module, const std::string& event, const std::string& msg, const char* f = nullptr, int l = 0, const char* fn = nullptr);
    void warning(const std::string& module, const std::string& event, const std::string& msg, const char* f = nullptr, int l = 0, const char* fn = nullptr);
    void error(const std::string& module, const std::string& event, const std::string& msg, const char* f = nullptr, int l = 0, const char* fn = nullptr);
    void critical(const std::string& module, const std::string& event, const std::string& msg, const char* f = nullptr, int l = 0, const char* fn = nullptr);

    // Accès aux 100 derniers événements en mémoire
    std::vector<LogEntry> recentEvents() const;
    std::vector<LogEntry> recentEntries() const { return recentEvents(); }

    // Gestion des écouteurs (IHM / Dock)
    void addListener(const std::string& name, LogListener listener);
    void removeListener(const std::string& name);

    // Forçage de synchronisation disque
    void flush();

    // Installation du bridge pour capturer qDebug(), qWarning(), etc.
    static void installQtMessageHandler();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string generateSessionId();
    void rotateOldSessions(size_t maxSessionsToKeep = 10);
    static std::string currentIsoTimestamp();

private:
    std::string m_logsDirectory;
    std::string m_sessionId;
    std::string m_sessionLogPath;
    std::string m_latestLogPath;

    std::ofstream m_fileStream;
    mutable std::mutex m_mutex;

    std::atomic<bool> m_isInitialized{false};
    std::atomic<bool> m_developerMode{false};
    std::atomic<uint64_t> m_sequenceCounter{0};

    RingBuffer<100> m_ringBuffer;

    mutable std::mutex m_commandMutex;
    CommandInfo m_lastCommand;

    mutable std::mutex m_listenersMutex;
    std::map<std::string, LogListener> m_listeners;
};

// Macros pratiques pour logger avec fichier et ligne automatique
#define TSA_LOG_TRACE(module, event, msg) \
    TSA::Diagnostics::Logger::instance().trace(module, event, msg, __FILE__, __LINE__, __FUNCTION__)

#define TSA_LOG_DEBUG(module, event, msg) \
    TSA::Diagnostics::Logger::instance().debug(module, event, msg, __FILE__, __LINE__, __FUNCTION__)

#define TSA_LOG_INFO(module, event, msg) \
    TSA::Diagnostics::Logger::instance().info(module, event, msg, __FILE__, __LINE__, __FUNCTION__)

#define TSA_LOG_WARN(module, event, msg) \
    TSA::Diagnostics::Logger::instance().warning(module, event, msg, __FILE__, __LINE__, __FUNCTION__)

#define TSA_LOG_ERROR(module, event, msg) \
    TSA::Diagnostics::Logger::instance().error(module, event, msg, __FILE__, __LINE__, __FUNCTION__)

#define TSA_LOG_CRIT(module, event, msg) \
    TSA::Diagnostics::Logger::instance().critical(module, event, msg, __FILE__, __LINE__, __FUNCTION__)

} // namespace TSA::Diagnostics
