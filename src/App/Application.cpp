#include "Application.h"
#include "../UI/MainWindow.h"
#include "../UI/Theme/ThemeManager.h"
#include "../Platform/WindowsAssociation.h"

#include <QStyleFactory>
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
#ifdef _WIN32
    // Association explicite pour afficher l'icône sur la barre des tâches de Windows
    initWindowsAppUserModelID();
#endif

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

Application::~Application() = default;

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

    m_mainWindow = std::make_unique<MainWindow>();

    for (int i = 1; i < args.size(); ++i)
    {
        if (args[i].endsWith(".tsa", Qt::CaseInsensitive))
        {
            m_mainWindow->loadFile(args[i]);
            break;
        }
    }

    m_mainWindow->show();
    return true;
}
