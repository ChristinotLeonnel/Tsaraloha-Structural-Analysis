#include "Application.h"
#include "../UI/Shell/AppShell.h"
#include "../UI/Theme/ThemeManager.h"
#include "../Platform/WindowsAssociation.h"

#include "../Diagnostics/Logger.h"
#include "../Diagnostics/CrashHandler.h"

#include <QStyleFactory>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <QDir>
#include <QIcon>

#ifdef _WIN32
#include <windows.h>
static void initWindowsAppUserModelID()
{
    typedef HRESULT (WINAPI *SetAppIdFunc)(PCWSTR);
    HMODULE hShell = LoadLibraryW(L"shell32.dll");
    if (hShell)
    {
        SetAppIdFunc pFunc = reinterpret_cast<SetAppIdFunc>(GetProcAddress(hShell, "SetCurrentProcessExplicitAppUserModelID"));
        if (pFunc)
        {
            pFunc(L"TSAEngineering.TSA.StructuralModeler.1.0");
        }
        FreeLibrary(hShell);
    }
}
#endif

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // 1. Initialiser immédiatement le système central de logging et de crash reporting
    TSA::Diagnostics::Logger::instance().init();
    TSA::Diagnostics::Logger::installQtMessageHandler();
    TSA::Diagnostics::CrashHandler::install();

    TSA_LOG_INFO("App", "ApplicationStarted", "Démarrage de l'application TSA v0.1.0");

#ifdef _WIN32
    // Association explicite pour afficher l'icône sur la barre des tâches de Windows
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

    setApplicationName("TSA");
    setOrganizationName("TSA Engineering");
    setApplicationVersion("0.1.0");

    QIcon appIcon;
    appIcon.addFile(":/icons/TSA.ico");
    appIcon.addFile(":/icons/TSA.svg");
    setWindowIcon(appIcon);

    // Thème moderne AutoCAD 2024 Dark pour logiciel technique
    setStyle(QStyleFactory::create("Fusion"));

    TSA::UI::ThemeManager::instance().setDarkMode(true, true);

    // Configuration automatique de l'environnement OpenCASCADE (ressources et shaders)
    if (qEnvironmentVariableIsEmpty("CSF_OCCTResourcePath"))
    {
        QString appDir = applicationDirPath();
        QDir resDir(appDir + "/../../opencascade-8.0.1-vc14-64/src");
        if (resDir.exists())
        {
            qputenv("CSF_OCCTResourcePath", resDir.absolutePath().toLocal8Bit());
            qputenv("CSF_OCCTShadersPath", (resDir.absolutePath() + "/OpenGl").toLocal8Bit());
        }
    }
}

Application::~Application()
{
    TSA_LOG_INFO("App", "ApplicationClosing", "Fermeture normale de l'application TSA");
    TSA::Diagnostics::CrashHandler::uninstall();
    TSA::Diagnostics::Logger::instance().shutdown();
}

bool Application::init()
{
#ifdef _WIN32
    // Enregistrement automatique de l'association .tsa pour l'utilisateur courant (Robot SA style)
    TSA::Platform::WindowsAssociation::registerFileAssociation();
#endif

    const QStringList args = arguments();
    for (int i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--register-associations")
        {
#ifdef _WIN32
            TSA::Platform::WindowsAssociation::registerFileAssociation();
#endif
            return false;
        }
        if (args[i] == "--unregister-associations")
        {
#ifdef _WIN32
            TSA::Platform::WindowsAssociation::unregisterFileAssociation();
#endif
            return false;
        }
    }

    // Lancement : Start Center seul ; le workspace de modélisation est créé à l'ouverture d'un projet.
    m_shell = std::make_unique<TSA::UI::AppShell>();
    m_shell->show();

    for (int i = 1; i < args.size(); ++i)
    {
        QString arg = args[i].trimmed();
        if (arg.startsWith('"') && arg.endsWith('"') && arg.length() >= 2)
        {
            arg = arg.mid(1, arg.length() - 2);
        }
        if (arg.endsWith(".tsa", Qt::CaseInsensitive))
        {
            m_shell->openProjectFile(arg);
            break;
        }
    }

    return true;
}
