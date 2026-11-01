#include "WorkspaceView.h"

#include "UI/Blueprint/BlueprintEditor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>

namespace TSA::UI
{

WorkspaceView::WorkspaceView(QWidget* modelView, QWidget* parent)
    : QWidget(parent)
    , m_modelView(modelView)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_split = new QSplitter(Qt::Horizontal, this);
    m_split->setObjectName(QStringLiteral("workspaceSplit"));
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(5);
    layout->addWidget(m_split);
    if (m_modelView)
    {
        // Vue partagée : la vue 3D garde une surface utile (sinon, en partage vertical, la fenêtre OCCT pouvait tomber à
        // 0 px de haut sous le Blueprint ; le séparateur ne garantissait que la barre du conteneur).
        m_modelView->setMinimumSize(m_modelView->minimumSize().expandedTo(QSize(240, 160)));
        m_split->addWidget(m_modelView);
    }
    connect(m_split, &QSplitter::splitterMoved, this, [this] {
        if (m_mode == Mode::Split) m_splitSizes = m_split->sizes();
    });
}

void WorkspaceView::setSession(TSA::Project::ProjectSession* session)
{
    m_session = session;
    if (m_blueprint) m_blueprint->setSession(session);
}

BlueprintEditor* WorkspaceView::blueprint()
{
    if (!m_blueprint)
    {
        m_blueprint = new BlueprintEditor(m_split);
        m_blueprint->setObjectName(QStringLiteral("workspaceBlueprint"));
        m_blueprint->setSession(m_session);
        m_split->addWidget(m_blueprint);
        m_split->setStretchFactor(0, 1);
        m_split->setStretchFactor(1, 1);
        if (!m_pendingEditorState.isEmpty()) m_blueprint->restoreLayoutState(m_pendingEditorState);
        m_pendingEditorState.clear();
        m_blueprint->setVisible(m_mode != Mode::Model3D);
        emit blueprintCreated(m_blueprint);
    }
    return m_blueprint;
}

void WorkspaceView::setMode(Mode mode)
{
    if (mode != Mode::Model3D) blueprint();   // l'éditeur existe dès qu'il doit être visible
    if (m_mode == Mode::Split && mode != Mode::Split) m_splitSizes = m_split->sizes();
    const bool changed = mode != m_mode;
    m_mode = mode;
    applyMode();
    if (changed) emit modeChanged(m_mode);
}

void WorkspaceView::applyMode()
{
    // Masquer (et non réduire à zéro) : la vue OCCT cachée ne redessine pas ; son contexte reste valide (la fenêtre
    // native n'est pas détruite, BUG-119).
    if (m_modelView) m_modelView->setVisible(m_mode != Mode::Blueprint);
    if (m_blueprint) m_blueprint->setVisible(m_mode != Mode::Model3D);
    if (m_mode == Mode::Split && m_blueprint)
    {
        const int total = m_split->orientation() == Qt::Horizontal ? m_split->width() : m_split->height();
        const bool usable = m_splitSizes.size() == 2 && m_splitSizes[0] > 0 && m_splitSizes[1] > 0;
        m_split->setSizes(usable ? m_splitSizes : QList<int>{ std::max(1, total / 2), std::max(1, total / 2) });
    }
    if (m_mode == Mode::Blueprint && m_blueprint) m_blueprint->setFocus();
    else if (m_mode == Mode::Model3D && m_modelView) m_modelView->setFocus();
}

Qt::Orientation WorkspaceView::splitOrientation() const
{
    return m_split->orientation();
}

void WorkspaceView::setSplitOrientation(Qt::Orientation orientation)
{
    if (orientation == m_split->orientation()) return;
    m_split->setOrientation(orientation);
    m_splitSizes.clear();   // proportions recalculées dans la nouvelle orientation
    applyMode();
}

void WorkspaceView::resetLayout()
{
    m_splitSizes.clear();
    m_split->setOrientation(Qt::Horizontal);
    if (m_blueprint)
    {
        m_blueprint->setPaletteVisible(true);
        m_blueprint->setInspectorVisible(true);
        m_blueprint->setMinimapVisible(true);
    }
    setMode(Mode::Model3D);
}

QString WorkspaceView::modeKey(Mode mode)
{
    switch (mode)
    {
    case Mode::Blueprint: return QStringLiteral("blueprint");
    case Mode::Split: return QStringLiteral("split");
    case Mode::Model3D: break;
    }
    return QStringLiteral("model3d");
}

bool WorkspaceView::modeFromKey(const QString& key, Mode* mode)
{
    for (Mode m : { Mode::Model3D, Mode::Blueprint, Mode::Split })
    {
        if (modeKey(m) == key)
        {
            *mode = m;
            return true;
        }
    }
    return false;
}

QJsonObject WorkspaceView::stateJson() const
{
    QJsonObject o;
    o["mode"] = modeKey(m_mode);
    o["orientation"] = m_split->orientation() == Qt::Vertical ? QStringLiteral("vertical") : QStringLiteral("horizontal");
    QJsonArray sizes;
    for (int s : (m_mode == Mode::Split ? m_split->sizes() : m_splitSizes)) sizes.append(s);
    o["splitSizes"] = sizes;
    const QByteArray editor = m_blueprint ? m_blueprint->saveLayoutState() : m_pendingEditorState;
    if (!editor.isEmpty()) o["blueprintLayout"] = QString::fromLatin1(editor.toBase64());
    return o;
}

void WorkspaceView::applyStateJson(const QJsonObject& state)
{
    Mode mode = Mode::Model3D;
    if (!modeFromKey(state.value("mode").toString(), &mode)) mode = Mode::Model3D;
    m_split->setOrientation(state.value("orientation").toString() == QLatin1String("vertical") ? Qt::Vertical : Qt::Horizontal);
    m_splitSizes.clear();
    const QJsonArray sizes = state.value("splitSizes").toArray();
    if (sizes.size() == 2)
    {
        const int a = sizes[0].toInt(0), b = sizes[1].toInt(0);
        if (a > 0 && b > 0) m_splitSizes = { a, b };
    }
    const QByteArray editorState = QByteArray::fromBase64(state.value("blueprintLayout").toString().toLatin1());
    if (m_blueprint) m_blueprint->restoreLayoutState(editorState);
    else m_pendingEditorState = editorState;
    setMode(mode);
}

void WorkspaceView::saveState(QSettings& settings) const
{
    // Mêmes clés qu'auparavant (réglages existants des utilisateurs relus tels quels).
    const QJsonObject o = stateJson();
    settings.beginGroup(QStringLiteral("Workspace"));
    settings.setValue(QStringLiteral("mode"), o.value("mode").toString());
    settings.setValue(QStringLiteral("orientation"), o.value("orientation").toString());
    settings.setValue(QStringLiteral("splitSizes"), o.value("splitSizes").toArray().toVariantList());
    if (m_blueprint) settings.setValue(QStringLiteral("blueprintLayout"), m_blueprint->saveLayoutState());
    settings.endGroup();
}

void WorkspaceView::restoreState(QSettings& settings)
{
    settings.beginGroup(QStringLiteral("Workspace"));
    QJsonObject o;
    o["mode"] = settings.value(QStringLiteral("mode")).toString();
    o["orientation"] = settings.value(QStringLiteral("orientation")).toString();
    QJsonArray sizes;   // valeurs relues en entiers (un fichier INI les rend sous forme de texte)
    for (const QVariant& v : settings.value(QStringLiteral("splitSizes")).toList()) sizes.append(v.toInt());
    o["splitSizes"] = sizes;
    o["blueprintLayout"] = QString::fromLatin1(settings.value(QStringLiteral("blueprintLayout")).toByteArray().toBase64());
    settings.endGroup();
    applyStateJson(o);
}

} // namespace TSA::UI
