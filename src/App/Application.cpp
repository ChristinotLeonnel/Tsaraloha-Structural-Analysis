#include "Application.h"
#include "../UI/Shell/AppShell.h"

Application::Application(int& argc, char** argv)
    : TSA::UI::EcosystemApplication(argc, argv)
{
}

Application::~Application() = default;

bool Application::init()
{
    if (!registerPlatformIntegration()) return false;

    // Lancement : Start Center seul ; le workspace de modélisation est créé à l'ouverture d'un projet.
    m_shell = std::make_unique<TSA::UI::AppShell>();
    m_shell->show();

    const QStringList files = projectFilesFromArguments();
    if (!files.isEmpty()) m_shell->openProjectFile(files.first());
    return true;
}
