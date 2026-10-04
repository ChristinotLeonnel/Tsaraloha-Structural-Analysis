// Fichier exclu du PCH (CMakeLists) : inclut <windows.h> / <dpapi.h> avec ses propres réglages.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dpapi.h>
#endif

#include "AISettings.h"

#include <QByteArray>

namespace TSA::AI::SecretStore
{

bool isProtected()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

QByteArray protect(const QByteArray& plain)
{
#ifdef _WIN32
    DATA_BLOB in{ static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data())) };
    DATA_BLOB out{ 0, nullptr };
    if (!CryptProtectData(&in, L"TSA AI", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
    QByteArray result(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
#else
    return plain;
#endif
}

QByteArray unprotect(const QByteArray& cipher)
{
#ifdef _WIN32
    DATA_BLOB in{ static_cast<DWORD>(cipher.size()), reinterpret_cast<BYTE*>(const_cast<char*>(cipher.data())) };
    DATA_BLOB out{ 0, nullptr };
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return {};
    QByteArray result(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
#else
    return cipher;
#endif
}

} // namespace TSA::AI::SecretStore
