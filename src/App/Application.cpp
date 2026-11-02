#include "Application.h"
#include "../UI/Shell/AppShell.h"

#include <QTimer>

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
    // Fermeture brutale précédente : projets modifiés à récupérer (BUG-157), une fois la fenêtre affichée
    QTimer::singleShot(0, m_shell.get(), [shell = m_shell.get()] { shell->offerRecovery(); });
    return true;
}
