#pragma once

#include "../UI/Common/EcosystemApplication.h"

#include <memory>

namespace TSA::UI { class AppShell; }
namespace TSA::Platform { class SingleInstance; }

/// Application TSA : initialisation commune de l'écosystème (EcosystemApplication) + fenêtre AppShell.
class Application : public TSA::UI::EcosystemApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    /// Faux : ne pas entrer dans la boucle d'événements (commande sans fenêtre, ou fichiers confiés à l'instance en cours).
    bool init();
    /// Une instance tournait déjà : elle a pris les fichiers de ce lancement, qui se termine normalement.
    bool forwardedToRunningInstance() const { return m_forwarded; }

private:
    void receiveFiles(const QStringList& files);

    std::unique_ptr<TSA::Platform::SingleInstance> m_instance;   ///< une seule fenêtre de TSA, comme AutoCAD
    std::unique_ptr<TSA::UI::AppShell> m_shell;
    QStringList m_receivedBeforeShell;
    bool m_forwarded = false;
};
