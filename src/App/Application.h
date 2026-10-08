#pragma once

#include "../UI/Common/EcosystemApplication.h"

#include <memory>

namespace TSA::UI { class AppShell; }

/// Application TSA : initialisation commune de l'écosystème (EcosystemApplication) + fenêtre AppShell.
class Application : public TSA::UI::EcosystemApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    bool init();

private:
    std::unique_ptr<TSA::UI::AppShell> m_shell;
};
