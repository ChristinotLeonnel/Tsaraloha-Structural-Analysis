#include "App/Application.h"

#include <QProcess>

int main(int argc, char* argv[])
{
    int code = 0;
    bool restart = false;
    QStringList projects;
    QString executable;
    {
        Application app(argc, argv);
        if (!app.init()) return -1;
        code = app.exec();
        restart = app.restartRequested();
        projects = app.restartProjectFiles();
        executable = QCoreApplication::applicationFilePath();
    }
    // Redémarrage demandé (langue de l'interface, docs/I18N.md) : relancé seulement après la fermeture complète de cette
    // instance (réglages écrits, canal d'automatisation et processus auxiliaires libérés)
    if (restart) QProcess::startDetached(executable, projects);
    return code;
}

