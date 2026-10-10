#include "EcosystemApplication.h"
#include "../../Core/AppPaths.h"

#include "../../Modules/ModuleRegistry.h"
#include "../../Plugins/PluginManager.h"
#include "../../Templates/TemplateRepository.h"

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

TSA::Modules::ModuleHostServices EcosystemApplication::moduleHostServices()
{
    TSA::Modules::ModuleHostServices s;
    s.loadPlugin = [](const QString& dll, QString* error) { return TSA::Plugins::PluginManager::instance().loadFile(dll, error); };
    s.stopPlugin = [](const QString& dll) { TSA::Plugins::PluginManager::instance().shutdownFile(dll); };
    s.registerTemplate = [](const QString& path, const QString& moduleId, QString* error) {
        return TSA::Templates::TemplateRepository::instance().registerModulePackage(path, moduleId, error);
    };
    s.unregisterTemplates = [](const QString& moduleId) { TSA::Templates::TemplateRepository::instance().unregisterModule(moduleId); };
    return s;
}

EcosystemApplication::EcosystemApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // 0. Identité d'abord : elle fixe les dossiers QStandardPaths (journaux, configuration, données).
    setApplicationName(TSA::Product::name());
    setOrganizationName(QString::fromLatin1(TSA::Product::kOrganizationName));
    if (TSA::Product::kOrganizationDomain[0] != '\0')
        setOrganizationDomain(QString::fromLatin1(TSA::Product::kOrganizationDomain));
    setApplicationVersion(TSA::Product::version());

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
        // Paquet : <app>/resources/occt ; développement : SDK OCCT des sources (Core/AppPaths).
        QDir resDir(TSA::Core::AppPaths::occtResourcesDir());
        if (resDir.exists())
        {
            qputenv("CSF_OCCTResourcePath", resDir.absolutePath().toLocal8Bit());
            qputenv("CSF_OCCTShadersPath", (resDir.absolutePath() + "/OpenGl").toLocal8Bit());
        }
    }

    // Plugins (<dossier de l'application>/plugins) : commandes et nœuds Blueprint ajoutés aux registres
    // globaux avant toute fenêtre (console, Blueprint et IA les voient comme les commandes intégrées).
    auto& plugins = TSA::Plugins::PluginManager::instance();
    plugins.loadDirectory(TSA::Plugins::PluginManager::defaultDirectory());
    for (const auto& p : plugins.plugins())
        TSA_LOG_INFO("App", p.loaded ? "PluginLoaded" : "PluginRejected",
                     (p.loaded ? "Plugin chargé : " + p.info.name + " " + p.info.version
                               : "Plugin refusé : " + p.path.toStdString() + " — " + p.error.toStdString()));

    // Modules (docs/SDK.md) : livrés (<app>/modules, approuvés) et utilisateur (<données>/modules, activés
    // seulement après approbation). Arrêt dans l'ordre inverse à la fermeture, avant la notification des plugins.
    auto& modules = TSA::Modules::ModuleRegistry::instance();
    modules.setHost(TSA::Product::name(), TSA::Product::version());
    modules.discover({ TSA::Core::AppPaths::shippedModulesDir() }, { TSA::Core::AppPaths::userModulesDir() });
    modules.activate(moduleHostServices());
    for (const auto& m : modules.modules())
        TSA_LOG_INFO("App", "Module",
                     QStringLiteral("Module %1 %2 : %3%4")
                         .arg(m.manifest.id, m.manifest.version.toString(), TSA::Modules::moduleStateName(m.state),
                              m.messages.isEmpty() ? QString() : QStringLiteral(" — ") + m.messages.join(QStringLiteral(" ; ")))
                         .toStdString());
    connect(this, &QCoreApplication::aboutToQuit, this, [] {
        TSA::Modules::ModuleRegistry::instance().shutdown(moduleHostServices());
        TSA::Plugins::PluginManager::instance().shutdownAll();
    });
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
