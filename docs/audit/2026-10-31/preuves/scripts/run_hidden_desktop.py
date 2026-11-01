"""run_hidden_desktop.py <journal> <cwd> <commande...>

Lance la commande sur un bureau Windows séparé (CreateDesktopW) : aucune fenêtre ni console sur le bureau de
l'utilisateur ; les processus enfants (OpenSees) héritent de ce bureau. Sortie standard et erreurs dans <journal>.
"""
import ctypes
import subprocess
import sys
from ctypes import wintypes

log, cwd, cmd = sys.argv[1], sys.argv[2], sys.argv[3:]
k32 = ctypes.WinDLL("kernel32", use_last_error=True)
u32 = ctypes.WinDLL("user32", use_last_error=True)

u32.CreateDesktopW.restype = wintypes.HANDLE
u32.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
u32.CloseDesktop.argtypes = [wintypes.HANDLE]


class SECURITY_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("nLength", wintypes.DWORD), ("lpSecurityDescriptor", ctypes.c_void_p), ("bInheritHandle", wintypes.BOOL)]


class STARTUPINFOW(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("lpReserved", wintypes.LPWSTR), ("lpDesktop", wintypes.LPWSTR),
                ("lpTitle", wintypes.LPWSTR), ("dwX", wintypes.DWORD), ("dwY", wintypes.DWORD), ("dwXSize", wintypes.DWORD),
                ("dwYSize", wintypes.DWORD), ("dwXCountChars", wintypes.DWORD), ("dwYCountChars", wintypes.DWORD),
                ("dwFillAttribute", wintypes.DWORD), ("dwFlags", wintypes.DWORD), ("wShowWindow", wintypes.WORD),
                ("cbReserved2", wintypes.WORD), ("lpReserved2", ctypes.c_void_p), ("hStdInput", wintypes.HANDLE),
                ("hStdOutput", wintypes.HANDLE), ("hStdError", wintypes.HANDLE)]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [("hProcess", wintypes.HANDLE), ("hThread", wintypes.HANDLE), ("dwProcessId", wintypes.DWORD),
                ("dwThreadId", wintypes.DWORD)]


k32.CreateFileW.restype = wintypes.HANDLE
k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.POINTER(SECURITY_ATTRIBUTES),
                            wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
k32.CreateProcessW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, ctypes.c_void_p, ctypes.c_void_p, wintypes.BOOL,
                               wintypes.DWORD, ctypes.c_void_p, wintypes.LPCWSTR, ctypes.POINTER(STARTUPINFOW),
                               ctypes.POINTER(PROCESS_INFORMATION)]
k32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
k32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
k32.CloseHandle.argtypes = [wintypes.HANDLE]
k32.SetErrorMode(0x0001 | 0x0002 | 0x8000)   # erreurs système muettes, héritées par l'enfant

name = "tsa_hidden_tests"
desk = u32.CreateDesktopW(name, None, None, 0, 0x10000000, None)   # GENERIC_ALL
if not desk:
    print("CreateDesktopW a échoué :", ctypes.get_last_error())
    sys.exit(3)

sa = SECURITY_ATTRIBUTES(ctypes.sizeof(SECURITY_ATTRIBUTES), None, True)
out = k32.CreateFileW(log, 0x40000000, 0x1 | 0x2, ctypes.byref(sa), 2, 0x80, None)   # GENERIC_WRITE, CREATE_ALWAYS
nul = k32.CreateFileW("NUL", 0x80000000, 0x1 | 0x2, ctypes.byref(sa), 3, 0x80, None)   # GENERIC_READ, OPEN_EXISTING

si = STARTUPINFOW()
si.cb = ctypes.sizeof(STARTUPINFOW)
si.lpDesktop = name
si.dwFlags = 0x100 | 0x1   # STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW
si.wShowWindow = 0         # SW_HIDE
si.hStdInput, si.hStdOutput, si.hStdError = nul, out, out
pi = PROCESS_INFORMATION()
cmdline = ctypes.create_unicode_buffer(subprocess.list2cmdline(cmd))
if not k32.CreateProcessW(None, cmdline, None, None, True, 0x08000000, None, cwd, ctypes.byref(si), ctypes.byref(pi)):
    print("CreateProcessW a échoué :", ctypes.get_last_error())
    sys.exit(3)
k32.WaitForSingleObject(pi.hProcess, 0xFFFFFFFF)
code = wintypes.DWORD()
k32.GetExitCodeProcess(pi.hProcess, ctypes.byref(code))
for h in (pi.hProcess, pi.hThread, out, nul):
    k32.CloseHandle(h)
u32.CloseDesktop(desk)
print("code", code.value)
sys.exit(0 if code.value == 0 else 1)
