#include "Logger.h"
#include "../Core/AppPaths.h"
#include <ProductIdentity.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <iostream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QSysInfo>

namespace fs = std::filesystem;

namespace TSA::Diagnostics
{

static void qtMessageHandlerBridge(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    LogLevel level = LogLevel::Info;
    switch (type)
    {
    case QtDebugMsg:    level = LogLevel::Debug; break;
    case QtInfoMsg:     level = LogLevel::Info; break;
    case QtWarningMsg:  level = LogLevel::Warning; break;
    case QtCriticalMsg: level = LogLevel::Error; break;
    case QtFatalMsg:    level = LogLevel::Critical; break;
    }

    std::string module = "Qt";
    if (context.category && std::string(context.category) != "default")
    {
        module = context.category;
    }

    Logger::instance().log(
        level,
        module,
        "QtSystemMessage",
        msg.toStdString(),
        context.file,
        context.line,
        context.function
    );
}

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger() = default;

Logger::~Logger()
{
    shutdown();
}

std::string Logger::currentIsoTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuffer{};
#if defined(_WIN32)
    localtime_s(&tmBuffer, &t);
#else
    localtime_r(&t, &tmBuffer);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuffer);
    std::ostringstream oss;
    oss << buf << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::generateSessionId()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuffer{};
#if defined(_WIN32)
    localtime_s(&tmBuffer, &t);
    uint32_t pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
    localtime_r(&t, &tmBuffer);
    uint32_t pid = 1;
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tmBuffer);
    std::ostringstream oss;
    oss << "session_" << buf << "_" << std::setfill('0') << std::setw(4) << (pid % 10000);
    return oss.str();
}

bool Logger::init(const std::string& baseDir)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isInitialized.load())
    {
        return true;
    }

    // Déterminer le dossier de logs
    if (!baseDir.empty())
    {
        m_logsDirectory = baseDir;
    }
    else
    {
        // Développement : logs/ du dossier de compilation ; installation : données utilisateur (le dossier
        // de l'application n'est pas inscriptible sous Program Files). Voir Core/AppPaths.
        m_logsDirectory = TSA::Core::AppPaths::logsDir().toStdString();
    }

    try
    {
        fs::create_directories(m_logsDirectory);
        fs::create_directories(m_logsDirectory + "/sessions");
    }
    catch (...)
    {
        // Permissions refusées : dossier temporaire de l'utilisateur (jamais le répertoire courant).
        m_logsDirectory = (QDir::tempPath() + "/TSA/logs").toStdString();
        try {
            fs::create_directories(m_logsDirectory + "/sessions");
        } catch (...) {}
    }

    m_sessionId = generateSessionId();

    // Nom de fichier de session : logs/sessions/session_YYYYMMDD_HHMMSS.log
    std::string timeStr = m_sessionId.substr(8, 15);
    std::replace(timeStr.begin(), timeStr.end(), '-', '_');
    m_sessionLogPath = m_logsDirectory + "/sessions/session_" + timeStr + ".log";
    m_latestLogPath = m_logsDirectory + "/tsa_latest.log";

    m_fileStream.open(m_sessionLogPath, std::ios::out | std::ios::trunc);
    if (!m_fileStream.is_open())
    {
        std::cerr << "[TSA::Logger] Impossible d'ouvrir le fichier de log : " << m_sessionLogPath << std::endl;
        return false;
    }

    m_isInitialized.store(true);

    // Écrire l'en-tête de session
    m_fileStream << "================================================================================\n";
    m_fileStream << TSA::Product::kName << " LOG SESSION STARTED\n";
    m_fileStream << "Session ID   : " << m_sessionId << "\n";
    m_fileStream << "Start Time   : " << currentIsoTimestamp() << "\n";
    m_fileStream << "Application  : " << TSA::Product::kName << " (" << TSA::Product::kLongName << ") v" << TSA::Product::kVersion << "\n";
    m_fileStream << "OS Version   : " << QSysInfo::prettyProductName().toStdString() << " (" << QSysInfo::currentCpuArchitecture().toStdString() << ")\n";
    m_fileStream << "Qt Version   : " << qVersion() << "\n";
    m_fileStream << "OCCT Version : 8.0.1\n";
#if defined(_DEBUG)
    m_fileStream << "Build Type   : Debug\n";
#else
    m_fileStream << "Build Type   : Release\n";
#endif
    m_fileStream << "================================================================================\n\n";
    m_fileStream.flush();

    // Rotation des anciennes sessions
    rotateOldSessions(10);

    return true;
}

void Logger::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isInitialized.load())
    {
        return;
    }

    if (m_fileStream.is_open())
    {
        m_fileStream << "\n================================================================================\n";
        m_fileStream << TSA::Product::kName << " LOG SESSION CLOSED : " << currentIsoTimestamp() << "\n";
        m_fileStream << "================================================================================\n";
        m_fileStream.flush();
        m_fileStream.close();
    }

    // Mettre à jour le fichier tsa_latest.log par copie du fichier de session actuel
    try
    {
        if (fs::exists(m_sessionLogPath))
        {
            fs::copy_file(m_sessionLogPath, m_latestLogPath, fs::copy_options::overwrite_existing);
        }
    }
    catch (...) {}

    m_isInitialized.store(false);
}

void Logger::setDeveloperMode(bool enabled)
{
    m_developerMode.store(enabled);
}

bool Logger::isDeveloperMode() const
{
    return m_developerMode.load();
}

void Logger::setLastCommand(const std::string& id, const std::string& name, const std::string& parameters)
{
    std::lock_guard<std::mutex> lock(m_commandMutex);
    m_lastCommand.id = id;
    m_lastCommand.name = name;
    m_lastCommand.parameters = parameters;
    m_lastCommand.startTime = currentIsoTimestamp();
}

void Logger::clearLastCommand()
{
    std::lock_guard<std::mutex> lock(m_commandMutex);
    m_lastCommand = CommandInfo{};
}

CommandInfo Logger::lastCommand() const
{
    std::lock_guard<std::mutex> lock(m_commandMutex);
    return m_lastCommand;
}

void Logger::log(LogLevel level,
                 const std::string& module,
                 const std::string& eventName,
                 const std::string& message,
                 const char* file,
                 int line,
                 const char* function)
{
    // Filtrage mode standard : ignorer Trace et Debug si developerMode est inactif
    if (!m_developerMode.load() && (level == LogLevel::Trace || level == LogLevel::Debug))
    {
        return;
    }

    LogEntry entry;
    entry.timestamp = currentIsoTimestamp();
    entry.level = level;
    entry.module = module;
    entry.eventName = eventName;
    entry.message = message;
    entry.file = file ? file : "";
    entry.line = line;
    entry.function = function ? function : "";
    entry.sequenceId = ++m_sequenceCounter;

    // Enregistrer dans le tampon circulaire en mémoire (indispensable pour diagnostic post-crash)
    m_ringBuffer.push(entry);

    std::string formatted = entry.formatForLog();

    // Écriture fichier thread-safe
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fileStream.is_open())
        {
            m_fileStream << formatted << "\n";
            // Forcer l'écriture immédiate sur disque en cas d'erreur ou d'événement critique
            if (level >= LogLevel::Error)
            {
                m_fileStream.flush();
            }
        }
    }

    // Notification des écouteurs (IHM, Console Dock)
    {
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        for (const auto& [name, listener] : m_listeners)
        {
            if (listener)
            {
                listener(entry);
            }
        }
    }
}

void Logger::trace(const std::string& module, const std::string& event, const std::string& msg, const char* f, int l, const char* fn)
{
    log(LogLevel::Trace, module, event, msg, f, l, fn);
}

void Logger::debug(const std::string& module, const std::string& event, const std::string& msg, const char* f, int l, const char* fn)
{
    log(LogLevel::Debug, module, event, msg, f, l, fn);
}

void Logger::info(const std::string& module, const std::string& event, const std::string& msg, const char* f, int l, const char* fn)
{
    log(LogLevel::Info, module, event, msg, f, l, fn);
}

void Logger::warning(const std::string& module, const std::string& event, const std::string& msg, const char* f, int l, const char* fn)
{
    log(LogLevel::Warning, module, event, msg, f, l, fn);
}

void Logger::error(const std::string& module, const std::string& event, const std::string& msg, const char* f, int l, const char* fn)
{
    log(LogLevel::Error, module, event, msg, f, l, fn);
}

void Logger::critical(const std::string& module, const std::string& event, const std::string& msg, const char* f, int l, const char* fn)
{
    log(LogLevel::Critical, module, event, msg, f, l, fn);
}

std::vector<LogEntry> Logger::recentEvents() const
{
    return m_ringBuffer.snapshot();
}

void Logger::addListener(const std::string& name, LogListener listener)
{
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    m_listeners[name] = listener;
}

void Logger::removeListener(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    m_listeners.erase(name);
}

void Logger::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open())
    {
        m_fileStream.flush();
    }
}

void Logger::installQtMessageHandler()
{
    qInstallMessageHandler(qtMessageHandlerBridge);
}

void Logger::rotateOldSessions(size_t maxSessionsToKeep)
{
    try
    {
        std::string sessDir = m_logsDirectory + "/sessions";
        if (!fs::exists(sessDir)) return;

        std::vector<fs::directory_entry> files;
        for (const auto& entry : fs::directory_iterator(sessDir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".log")
            {
                files.push_back(entry);
            }
        }

        if (files.size() > maxSessionsToKeep)
        {
            // Trier par date de dernière modification croissante (plus ancien en premier)
            std::sort(files.begin(), files.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
                return fs::last_write_time(a) < fs::last_write_time(b);
            });

            size_t deleteCount = files.size() - maxSessionsToKeep;
            for (size_t i = 0; i < deleteCount; ++i)
            {
                fs::remove(files[i].path());
            }
        }
    }
    catch (...)
    {
        // Tolérance d'échec sur la rotation (ne jamais bloquer le démarrage de l'application)
    }
}

} // namespace TSA::Diagnostics
