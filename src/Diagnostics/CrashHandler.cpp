#include "CrashHandler.h"
#include <ProductIdentity.h>
#include "Logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace fs = std::filesystem;

namespace TSA::Diagnostics
{

static bool s_isInstalled = false;

#ifdef _WIN32
static LPTOP_LEVEL_EXCEPTION_FILTER s_previousFilter = nullptr;
static std::terminate_handler s_previousTerminate = nullptr;

static const char* getExceptionName(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION (Violation d'accès mémoire)";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED (Dépassement d'indice de tableau)";
    case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO (Division par zéro flottante)";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW (Dépassement de capacité flottante)";
    case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION (Instruction machine invalide)";
    case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR (Erreur d'accès page mémoire)";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO (Division entière par zéro)";
    case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW (Débordement de pile d'appels)";
    case 0xE06D7363:                         return "Visual C++ C++ Exception (MSVC std::exception / OCCT Standard_Failure)";
    default:                                 return "Unknown Exception Code";
    }
}

static std::string generateStackTrace(CONTEXT* ctx)
{
    std::ostringstream oss;
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(process, NULL, TRUE))
    {
        oss << "  [SymInitialize failed: " << GetLastError() << "]\n";
        return oss.str();
    }

    STACKFRAME64 frame{};
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

#if defined(_M_X64) || defined(__x86_64__)
    frame.AddrPC.Offset = ctx->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = ctx->Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx->Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx->Esp;
    frame.AddrStack.Mode = AddrModeFlat;
#endif

    int frameIndex = 0;
    while (StackWalk64(machineType, process, thread, &frame, ctx, NULL,
                       SymFunctionTableAccess64, SymGetModuleBase64, NULL))
    {
        if (frame.AddrPC.Offset == 0) break;

        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)]{};
        PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        std::string funcName = "UnknownFunction";
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, pSymbol))
        {
            funcName = pSymbol->Name;
        }

        IMAGEHLP_LINE64 lineInfo{};
        lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisplacement = 0;
        std::string sourceInfo;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &lineInfo))
        {
            sourceInfo = std::string(lineInfo.FileName) + ":" + std::to_string(lineInfo.LineNumber);
        }

        oss << "  #" << std::setw(2) << frameIndex++ << " 0x" << std::hex << frame.AddrPC.Offset << std::dec
            << " in " << funcName;
        if (!sourceInfo.empty())
        {
            oss << " (" << sourceInfo << ")";
        }
        oss << "\n";

        if (frameIndex > 40) // Limiter à 40 frames pour éviter les boucles infinies
        {
            oss << "  ... (pile tronquée à 40 niveaux)\n";
            break;
        }
    }

    SymCleanup(process);
    return oss.str();
}

static void createMiniDump(EXCEPTION_POINTERS* ep, const std::string& dumpPath)
{
    HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithUnloadedModules),
            ep ? &mei : NULL,
            NULL,
            NULL
        );
        CloseHandle(hFile);
    }
}

struct CrashReportJob
{
    const char* reason;
    EXCEPTION_POINTERS* ep;
};

static DWORD WINAPI crashReportThreadProc(LPVOID param)
{
    auto* job = static_cast<CrashReportJob*>(param);
    CrashHandler::writeCrashReport(job->reason, job->ep);
    return 0;
}

static LONG WINAPI TSAUnhandledExceptionFilter(EXCEPTION_POINTERS* ep)
{
    const bool isStackOverflow = ep && ep->ExceptionRecord &&
                                 ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW;
    if (isStackOverflow)
    {
        // Pile épuisée : le rapport (StackWalk64, MiniDumpWriteDump, flux) ne peut pas s'exécuter
        // sur ce thread sans provoquer une seconde violation d'accès (rapports vides observés).
        // On le délègue à un thread disposant d'une pile neuve.
        CrashReportJob job{ "Débordement de pile (récursion infinie probable)", ep };
        HANDLE th = CreateThread(nullptr, 1024 * 1024, crashReportThreadProc, &job, 0, nullptr);
        if (th)
        {
            WaitForSingleObject(th, 30000);
            CloseHandle(th);
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }

    CrashHandler::writeCrashReport("Unhandled Exception (Crash natif détecté)", ep);

    std::string msg = std::string(TSA::Product::kName) + " a rencontré un problème critique inattendu.\n\n"
                      "Un rapport de crash technique et les 100 dernières actions ont été enregistrés dans :\n"
                      + Logger::instance().logsDirectory() + "\\tsa_crash.log\n\n"
                      "L'application va maintenant se fermer.";

    // Les chaînes du projet sont en UTF-8 (/utf-8) : MessageBoxA les interprétait dans la page de
    // code ANSI (« ArrÃªt d'urgence... »). Conversion explicite vers UTF-16 pour MessageBoxW.
    auto toWide = [](const std::string& utf8) {
        const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        std::wstring w(n > 0 ? static_cast<size_t>(n) : 0, L'\0');
        if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
        return w;
    };
    MessageBoxW(NULL, toWide(msg).c_str(), toWide(std::string(TSA::Product::kName) + " - Arrêt d'urgence suite à une anomalie critique").c_str(),
                MB_OK | MB_ICONERROR | MB_TASKMODAL);

    if (s_previousFilter)
    {
        return s_previousFilter(ep);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static void TSATerminateHandler()
{
    std::string reason = "std::terminate() appelé (Exception non interceptée)";
    std::exception_ptr ep = std::current_exception();
    if (ep)
    {
        try {
            std::rethrow_exception(ep);
        }
        catch (const std::exception& e) {
            reason += " - Exception: ";
            reason += e.what();
        }
        catch (...) {
            reason += " - Exception non dérivée de std::exception (ex: Standard_Failure OpenCASCADE)";
        }
    }
    CrashHandler::writeCrashReport(reason, nullptr);

    if (s_previousTerminate)
    {
        s_previousTerminate();
    }
    else
    {
        std::abort();
    }
}
#endif

void CrashHandler::install()
{
    if (s_isInstalled) return;

#ifdef _WIN32
    s_previousFilter = SetUnhandledExceptionFilter(TSAUnhandledExceptionFilter);
    s_previousTerminate = std::set_terminate(TSATerminateHandler);
#endif
    s_isInstalled = true;
    TSA_LOG_INFO("CrashHandler", "CrashHandlerInstalled", "Système de capture de crash Windows et minidump activé");
}

void CrashHandler::uninstall()
{
    if (!s_isInstalled) return;

#ifdef _WIN32
    if (s_previousFilter)
    {
        SetUnhandledExceptionFilter(s_previousFilter);
        s_previousFilter = nullptr;
    }
    if (s_previousTerminate)
    {
        std::set_terminate(s_previousTerminate);
        s_previousTerminate = nullptr;
    }
#endif
    s_isInstalled = false;
}

bool CrashHandler::isInstalled()
{
    return s_isInstalled;
}

void CrashHandler::writeCrashReport(const std::string& reason, void* exceptionPointers)
{
    std::string logsDir = Logger::instance().logsDirectory();
    if (logsDir.empty()) logsDir = "logs";

    try {
        fs::create_directories(logsDir);
    } catch (...) {}

    std::string crashLogPath = logsDir + "/tsa_crash.log";
    std::ofstream out(crashLogPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return;

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuffer{};
#ifdef _WIN32
    localtime_s(&tmBuffer, &t);
#else
    localtime_r(&t, &tmBuffer);
#endif
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmBuffer);

    out << "============================================================\n";
    out << TSA::Product::kName << " CRASH REPORT\n";
    out << "============================================================\n\n";

    out << "Session ID       : " << Logger::instance().sessionId() << "\n";
    out << "Crash Timestamp  : " << timeStr << "\n";
    out << "Application      : " << TSA::Product::kName << " (" << TSA::Product::kLongName << ") v" << TSA::Product::kVersion << "\n";
    out << "Reason           : " << reason << "\n";

#ifdef _WIN32
    out << "OS Platform      : Windows\n";
    if (exceptionPointers)
    {
        auto* ep = static_cast<EXCEPTION_POINTERS*>(exceptionPointers);
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        out << "Exception Code   : 0x" << std::hex << code << std::dec << " (" << getExceptionName(code) << ")\n";
        out << "Exception Address: " << addr << "\n";

        // Générer le MiniDump Windows
        char dumpFileStr[64];
        std::strftime(dumpFileStr, sizeof(dumpFileStr), "%Y%m%d_%H%M%S", &tmBuffer);
        std::string dumpPath = logsDir + "/crash_dump_" + dumpFileStr + ".dmp";
        createMiniDump(ep, dumpPath);
        out << "MiniDump File    : " << dumpPath << "\n";
    }
#endif

    // Dernière commande exécutée
    CommandInfo lastCmd = Logger::instance().lastCommand();
    out << "\n------------------------------------------------------------\n";
    out << "LAST EXECUTED COMMAND\n";
    out << "------------------------------------------------------------\n";
    if (!lastCmd.name.empty())
    {
        out << "Command Name     : " << lastCmd.name << "\n";
        out << "Command ID       : " << lastCmd.id << "\n";
        out << "Parameters       : " << lastCmd.parameters << "\n";
        out << "Start Timestamp  : " << lastCmd.startTime << "\n";
    }
    else
    {
        out << "Aucune commande en cours d'exécution au moment du crash.\n";
    }

    // 100 derniers événements en mémoire
    out << "\n------------------------------------------------------------\n";
    out << "LAST 100 EVENTS BEFORE CRASH (Chronological Order)\n";
    out << "------------------------------------------------------------\n";
    auto events = Logger::instance().recentEvents();
    if (events.empty())
    {
        out << "Aucun événement enregistré dans le tampon circulaire.\n";
    }
    else
    {
        for (size_t i = 0; i < events.size(); ++i)
        {
            out << "[" << std::setw(3) << (i + 1) << "] " << events[i].formatForLog() << "\n";
        }
    }

#ifdef _WIN32
    // Pile d'appels (Stack Trace)
    if (exceptionPointers)
    {
        auto* ep = static_cast<EXCEPTION_POINTERS*>(exceptionPointers);
        out << "\n------------------------------------------------------------\n";
        out << "STACK TRACE\n";
        out << "------------------------------------------------------------\n";
        out << generateStackTrace(ep->ContextRecord);
    }
#endif

    out << "\n============================================================\n";
    out << "END OF CRASH REPORT\n";
    out << "============================================================\n";
    out.flush();
    out.close();

    Logger::instance().flush();
}

} // namespace TSA::Diagnostics
