#include "WindowsAssociation.h"
#include "App/ProductInfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace TSA::Platform
{

bool WindowsAssociation::registerFileAssociation(const QString& executablePath)
{
#ifndef _WIN32
    Q_UNUSED(executablePath);
    return false;
#else
    QString appPath = executablePath;
    if (appPath.isEmpty())
    {
        appPath = QCoreApplication::applicationFilePath();
    }
    appPath = QDir::toNativeSeparators(appPath);

    if (appPath.isEmpty() || !QFileInfo::exists(appPath))
    {
        return false;
    }

    // Enregistrement dans HKCU\Software\Classes (ne requiert pas de privilèges Administrateur)
    QSettings reg("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);

    // Extension native et ProgID du produit compilé (<ProductIdentity.h>) : TSA → .tsa / TSA.Project,
    // TSALab → .tsalab / TSALab.Project. Un produit ne touche jamais à l'extension de l'autre.
    using namespace TSA::Product;
    const QString ext = QString::fromLatin1(kProjectExtension);
    const QString progId = QString::fromLatin1(kProgId);

    // 1. Association de l'extension native au ProgID du produit
    reg.setValue(ext + "/.", progId);
    reg.setValue(ext + "/Content Type", kProjectContentType);
    reg.setValue(ext + "/PerceivedType", "document");
    reg.setValue(ext + "/OpenWithProgids/" + progId, "");

    // 2. Définition du ProgID
    reg.setValue(progId + "/.", kProjectFriendlyName);
    reg.setValue(progId + "/FriendlyTypeName", kProjectFriendlyName);

    // Icône personnalisée associée au fichier (icône principale de l'exécutable)
    reg.setValue(progId + "/DefaultIcon/.", QString("\"%1\",0").arg(appPath));

    // Commande shell pour l'ouverture par double-clic
    reg.setValue(progId + "/shell/open/.", QStringLiteral("Ouvrir avec %1").arg(TSA::Product::name()));
    reg.setValue(progId + "/shell/open/command/.", "\"" + appPath + "\" \"%1\"");

    reg.sync();

    // 3. Miniatures dans l'Explorateur (extension <Produit>ThumbnailProvider.dll livrée à côté de l'exécutable).
    //    Réenregistrée à chaque lancement : suit un déplacement de l'installation, persiste après
    //    redémarrage (registre), sans droits administrateur.
    registerThumbnailProvider();

    // 4. Notifier le Shell Windows pour rafraîchir l'Explorateur immédiatement
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return true;
#endif
}

bool WindowsAssociation::unregisterFileAssociation()
{
#ifndef _WIN32
    return false;
#else
    unregisterThumbnailProvider();
    QSettings reg("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    reg.remove(QString::fromLatin1(TSA::Product::kProjectExtension));
    reg.remove(QString::fromLatin1(TSA::Product::kProgId));
    reg.sync();

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
#endif
}

#ifdef _WIN32
namespace
{
// Appelle une fonction d'enregistrement exportée par l'extension (une seule implémentation : la DLL).
bool callThumbnailProviderExport(const char* exportName)
{
    const QString dll = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/" + TSA::Product::kThumbnailProviderDll);
    if (!QFileInfo::exists(dll)) return false;
    HMODULE module = LoadLibraryExW(reinterpret_cast<LPCWSTR>(dll.utf16()), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) return false;
    using RegFn = HRESULT(STDAPICALLTYPE*)();
    auto fn = reinterpret_cast<RegFn>(GetProcAddress(module, exportName));
    const bool ok = fn && SUCCEEDED(fn());
    FreeLibrary(module);
    return ok;
}
} // namespace
#endif

bool WindowsAssociation::registerThumbnailProvider()
{
#ifdef _WIN32
    return callThumbnailProviderExport("DllRegisterServer");
#else
    return false;
#endif
}

bool WindowsAssociation::unregisterThumbnailProvider()
{
#ifdef _WIN32
    return callThumbnailProviderExport("DllUnregisterServer");
#else
    return false;
#endif
}

void WindowsAssociation::notifyFileUpdated(const QString& filePath)
{
#ifdef _WIN32
    // L'Explorateur invalide sa miniature en cache et la redemande à l'extension.
    const QString native = QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath());
    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, reinterpret_cast<LPCWSTR>(native.utf16()), nullptr);
#else
    Q_UNUSED(filePath);
#endif
}

bool WindowsAssociation::isFileAssociationRegistered()
{
#ifndef _WIN32
    return false;
#else
    QSettings reg("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    const QString progId = reg.value(QString::fromLatin1(TSA::Product::kProjectExtension) + "/.").toString();
    return (progId == QString::fromLatin1(TSA::Product::kProgId));
#endif
}

} // namespace TSA::Platform
