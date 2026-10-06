#pragma once

#include <QApplication>
#include <memory>

namespace TSA::UI { class AppShell; }

class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    bool init();

private:
    std::unique_ptr<TSA::UI::AppShell> m_shell;
};
