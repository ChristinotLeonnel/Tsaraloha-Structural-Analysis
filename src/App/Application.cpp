#include "Application.h"
#include "../UI/MainWindow.h"

#include <QStyleFactory>
#include <QDir>
#include <QIcon>

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("TSA");
    setOrganizationName("TSA Engineering");
    setApplicationVersion("0.1.0");
    setWindowIcon(QIcon(":/icons/TSA.svg"));

    // Thème moderne épuré pour logiciel technique
    setStyle(QStyleFactory::create("Fusion"));

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
    m_mainWindow = std::make_unique<MainWindow>();
    m_mainWindow->show();
    return true;
}
