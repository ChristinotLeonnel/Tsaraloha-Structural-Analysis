// Fichier exclu du PCH (CMakeLists) : inclut <windows.h> avec ses propres réglages.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "ProcessLifetime.h"

namespace TSA::AI
{

bool bindToParentLifetime(qint64 pid)
{
#ifdef _WIN32
    // Un Job Object unique pour toute la session TSA : à la fermeture de son dernier handle
    // (sortie normale OU plantage de TSA), Windows termine les processus rattachés.
    static HANDLE job = [] {
        HANDLE h = CreateJobObjectW(nullptr, nullptr);
        if (h)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(h, JobObjectExtendedLimitInformation, &info, sizeof(info));
        }
        return h;
    }();
    if (!job || pid <= 0) return false;
    HANDLE proc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!proc) return false;
    const bool ok = AssignProcessToJobObject(job, proc) != FALSE;
    CloseHandle(proc);
    return ok;
#else
    (void)pid;
    return false;
#endif
}

} // namespace TSA::AI
