#include "ProjectLayoutState.h"

#include "UI/WindowManager/WindowManager.h"
#include "UI/Workspace/WorkspaceView.h"

#include <QDockWidget>
#include <QJsonArray>
#include <QSet>
#include <QTabBar>

namespace TSA::UI
{

QJsonObject captureProjectLayout(const WindowManager& windows, const WorkspaceView* workspace, const QTabBar* ribbonTabs,
                                 const QList<const QDockWidget*>& temporarilyHidden)
{
    QJsonObject o { { "schema", 1 } };
    o["docks"] = QString::fromLatin1(windows.saveState().toBase64());
    QJsonArray open, closed;
    for (const WindowInfo& w : windows.allWindows())
    {
        if (!w.dockWidget) continue;
        // isHidden (drapeau explicite) et non isVisible : juste, même fenêtre principale masquée ou réduite.
        const bool shown = !w.dockWidget->isHidden() || temporarilyHidden.contains(w.dockWidget.data());
        (shown ? open : closed).append(w.id);
    }
    o["openDocks"] = open;
    o["closedDocks"] = closed;
    if (workspace) o["workspace"] = workspace->stateJson();
    if (ribbonTabs && ribbonTabs->count() > 0)
    {
        const int index = ribbonTabs->currentIndex();
        o["ribbonTab"] = QJsonObject { { "index", index }, { "title", ribbonTabs->tabText(index) } };
    }
    return o;
}

QStringList applyProjectLayout(WindowManager& windows, WorkspaceView* workspace, QTabBar* ribbonTabs, const QJsonObject& state)
{
    QStringList warnings;
    const int schema = state.value("schema").toInt(0);
    if (schema != 1)
    {
        warnings << QStringLiteral("disposition : version %1 inconnue, ignorée").arg(schema);
        return warnings;
    }

    const QByteArray docks = QByteArray::fromBase64(state.value("docks").toString().toLatin1());
    if (!docks.isEmpty() && !windows.restoreState(docks))
        warnings << QStringLiteral("état des panneaux refusé par Qt : liste des panneaux ouverts appliquée");

    QSet<QString> open, closed;
    for (const QJsonValue& v : state.value("openDocks").toArray()) open.insert(v.toString());
    for (const QJsonValue& v : state.value("closedDocks").toArray()) closed.insert(v.toString());
    for (const WindowInfo& w : windows.allWindows())
    {
        if (!w.dockWidget) continue;
        if (open.contains(w.id)) w.dockWidget->setVisible(true);
        else if (closed.contains(w.id)) w.dockWidget->setVisible(false);
    }

    if (workspace && state.value("workspace").isObject())
        workspace->applyStateJson(state.value("workspace").toObject());

    if (ribbonTabs && state.value("ribbonTab").isObject())
    {
        const QJsonObject tab = state.value("ribbonTab").toObject();
        const QString title = tab.value("title").toString();
        int found = -1;
        for (int i = 0; i < ribbonTabs->count() && found < 0; ++i)
            if (ribbonTabs->tabText(i) == title) found = i;
        if (found < 0)
        {
            const int index = tab.value("index").toInt(-1);
            if (index >= 0 && index < ribbonTabs->count()) found = index;
        }
        if (found >= 0) ribbonTabs->setCurrentIndex(found);   // le QTabWidget du ruban suit sa barre d'onglets
        else warnings << QStringLiteral("onglet du ruban « %1 » introuvable").arg(title);
    }
    return warnings;
}

} // namespace TSA::UI
