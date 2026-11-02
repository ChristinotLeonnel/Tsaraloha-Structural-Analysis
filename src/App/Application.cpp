#include "Application.h"
#include "../Platform/SingleInstance.h"
#include "../UI/Shell/AppShell.h"
#include "ProductInfo.h"

#include <QTimer>

Application::Application(int& argc, char** argv)
    : TSA::UI::EcosystemApplication(argc, argv)
{
}

Application::~Application() = default;

bool Application::init()
{
    const QStringList args = arguments();
    const QStringList files = projectFilesFromArguments();
    // Une seule fenêtre : un lancement suivant confie ses fichiers à l'instance en cours et se termine, sans rien afficher.
    // --new-instance : seconde fenêtre explicite (essais, comparaison de deux versions de TSA).
    const bool commandOnly = args.contains(QStringLiteral("--register-associations")) || args.contains(QStringLiteral("--unregister-associations"));
    if (!commandOnly && !args.contains(QStringLiteral("--new-instance")))
    {
        m_instance = std::make_unique<TSA::Platform::SingleInstance>();
        connect(m_instance.get(), &TSA::Platform::SingleInstance::filesReceived, this, [this](const QStringList& received) { receiveFiles(received); });
        if (m_instance->acquire(files) == TSA::Platform::SingleInstance::Role::Forwarded)
        {
            m_forwarded = true;
            return false;
        }
        connect(this, &QCoreApplication::aboutToQuit, m_instance.get(), &TSA::Platform::SingleInstance::stop);
    }

    if (!registerPlatformIntegration()) return false;

    // Lancement : Start Center seul ; le workspace de modélisation est créé à l'ouverture d'un projet.
    m_shell = std::make_unique<TSA::UI::AppShell>();
    m_shell->show();

    // Tous les fichiers du lancement (BUG : seul le premier était ouvert, y compris au redémarrage pour changer de langue)
    m_shell->openProjectFiles(files + m_receivedBeforeShell);
    // Fermeture brutale précédente : projets modifiés à récupérer (BUG-157), une fois la fenêtre affichée
    QTimer::singleShot(0, m_shell.get(), [shell = m_shell.get()] { shell->offerRecovery(); });
    return true;
}

void Application::receiveFiles(const QStringList& files)
{
    QStringList openable;
    for (const QString& file : files)
        if (TSA::Product::isOpenableProjectFile(file)) openable << file;
    if (!m_shell)
    {
        m_receivedBeforeShell << openable;
        return;
    }
    m_shell->bringToFront();
    m_shell->openProjectFiles(openable);
}
