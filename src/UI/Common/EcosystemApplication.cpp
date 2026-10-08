#include "EcosystemApplication.h"

#include "App/ProductInfo.h"
#include "../Theme/ThemeManager.h"
#include "../../Diagnostics/CrashHandler.h"
#include "../../Diagnostics/Logger.h"
#include "../../Platform/WindowsAssociation.h"

#include <QDir>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QStyleFactory>
#include <QTranslator>

#ifdef _WIN32
#include <windows.h>

namespace
{
// Association explicite pour afficher l'icône du produit sur la barre des tâches de Windows.
void initWindowsAppUserModelID()
{
    typedef HRESULT(WINAPI * SetAppIdFunc)(PCWSTR);
    HMODULE hShell = LoadLibraryW(L"shell32.dll");
    if (hShell)
    {
        SetAppIdFunc pFunc = reinterpret_cast<SetAppIdFunc>(GetProcAddress(hShell, "SetCurrentProcessExplicitAppUserModelID"));
        if (pFunc) pFunc(TSA::Product::kAppUserModelId);
        FreeLibrary(hShell);
    }
}
} // namespace
#endif

namespace TSA::UI
{

EcosystemApplication::EcosystemApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // 1. Initialiser immédiatement le système central de logging et de crash reporting
    TSA::Diagnostics::Logger::instance().init();
    TSA::Diagnostics::Logger::installQtMessageHandler();
    TSA::Diagnostics::CrashHandler::install();

    TSA_LOG_INFO("App", "ApplicationStarted",
                 std::string("Démarrage de l'application ") + TSA::Product::kName + " v" + TSA::Product::kVersion);

#ifdef _WIN32
    initWindowsAppUserModelID();
#endif

    // Textes standard de Qt en français (boutons « Enregistrer », « Annuler », boîtes de fichiers…) :
    // traduction déployée à côté de l'exécutable, sinon celle de l'installation Qt (BUG-020).
    auto* qtTranslator = new QTranslator(this);
    if (qtTranslator->load(QLocale(QLocale::French), QStringLiteral("qtbase"), QStringLiteral("_"),
                           applicationDirPath() + QStringLiteral("/translations"))
        || qtTranslator->load(QLocale(QLocale::French), QStringLiteral("qtbase"), QStringLiteral("_"),
                              QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        installTranslator(qtTranslator);
    }

    setApplicationName(TSA::Product::name());
    setOrganizationName(QString::fromLatin1(TSA::Product::kOrganizationName));
    if (TSA::Product::kOrganizationDomain[0] != '\0')
        setOrganizationDomain(QString::fromLatin1(TSA::Product::kOrganizationDomain));
    setApplicationVersion(TSA::Product::version());

    QIcon appIcon;
    appIcon.addFile(QString::fromLatin1(TSA::Product::kIconIco));
    appIcon.addFile(QString::fromLatin1(TSA::Product::kIconSvg));
    setWindowIcon(appIcon);

    // Thème moderne AutoCAD 2024 Dark pour logiciel technique
    setStyle(QStyleFactory::create("Fusion"));
    ThemeManager::instance().setDarkMode(true, true);

    // Configuration automatique de l'environnement OpenCASCADE (ressources et shaders)
    if (qEnvironmentVariableIsEmpty("CSF_OCCTResourcePath"))
    {
        QDir resDir(applicationDirPath() + "/../../opencascade-8.0.1-vc14-64/src");
        if (!resDir.exists() && !TSA::Product::sourceDirectory().isEmpty()) // SDK de la base commune
            resDir.setPath(TSA::Product::sourceDirectory() + "/opencascade-8.0.1-vc14-64/src");
        if (resDir.exists())
        {
            qputenv("CSF_OCCTResourcePath", resDir.absolutePath().toLocal8Bit());
            qputenv("CSF_OCCTShadersPath", (resDir.absolutePath() + "/OpenGl").toLocal8Bit());
        }
    }
}

EcosystemApplication::~EcosystemApplication()
{
    TSA_LOG_INFO("App", "ApplicationClosing", std::string("Fermeture normale de l'application ") + TSA::Product::kName);
    TSA::Diagnostics::CrashHandler::uninstall();
    TSA::Diagnostics::Logger::instance().shutdown();
}

QStringList EcosystemApplication::projectFilesFromArguments() const
{
    QStringList files;
    const QStringList args = arguments();
    for (int i = 1; i < args.size(); ++i)
    {
        QString arg = args[i].trimmed();
        if (arg.startsWith('"') && arg.endsWith('"') && arg.length() >= 2) arg = arg.mid(1, arg.length() - 2);
        if (TSA::Product::isOpenableProjectFile(arg)) files << arg;
    }
    return files;
}

bool EcosystemApplication::registerPlatformIntegration()
{
#ifdef _WIN32
    const QStringList args = arguments();
    if (args.contains(QStringLiteral("--register-associations")))
    {
        TSA::Platform::WindowsAssociation::registerFileAssociation();
        return false;
    }
    if (args.contains(QStringLiteral("--unregister-associations")))
    {
        TSA::Platform::WindowsAssociation::unregisterFileAssociation();
        return false;
    }
    TSA::Platform::WindowsAssociation::registerFileAssociation();
#endif
    return true;
}

} // namespace TSA::UI
