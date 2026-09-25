#include "WindowsAssociation.h"

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

    // 1. Association de l'extension .tsa au ProgID TSA.Project
    reg.setValue(".tsa/.", "TSA.Project");
    reg.setValue(".tsa/Content Type", "application/x-tsa-project");
    reg.setValue(".tsa/PerceivedType", "document");
    reg.setValue(".tsa/OpenWithProgids/TSA.Project", "");

    // 2. Définition du ProgID TSA.Project
    reg.setValue("TSA.Project/.", "TSA Project File");
    reg.setValue("TSA.Project/FriendlyTypeName", "TSA Project File");
    
    // Icône personnalisée associée au fichier (icône principale de l'exécutable TSA.exe)
    reg.setValue("TSA.Project/DefaultIcon/.", QString("\"%1\",0").arg(appPath));

    // Commande shell pour l'ouverture par double-clic
    reg.setValue("TSA.Project/shell/open/.", "Ouvrir avec TSA");
    reg.setValue("TSA.Project/shell/open/command/.", QString("\"%1\" \"%%1\"").arg(appPath));

    reg.sync();

    // 3. Notifier le Shell Windows pour rafraîchir l'Explorateur immédiatement
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return true;
#endif
}

bool WindowsAssociation::unregisterFileAssociation()
{
#ifndef _WIN32
    return false;
#else
    QSettings reg("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    reg.remove(".tsa");
    reg.remove("TSA.Project");
    reg.sync();

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
#endif
}

bool WindowsAssociation::isFileAssociationRegistered()
{
#ifndef _WIN32
    return false;
#else
    QSettings reg("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    QString progId = reg.value(".tsa/.").toString();
    return (progId == "TSA.Project");
#endif
}

} // namespace TSA::Platform
