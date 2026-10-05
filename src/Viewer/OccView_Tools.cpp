// Saisie 3D des outils de modification / dessin (TSA::Interaction::ModelingTool) :
// clics (point accroché ou barre sous le curseur), aperçu (traits + fantômes de la sélection),
// valeur tapée au clavier (Entrée), Échap. L'opération elle-même est exécutée par MainWindow
// (transaction) quand l'outil est prêt : signal modelingToolReady().

#include "OccView.h"

#include "../Interaction/Tools/ModelingTool.h"
#include "../Model/Model.h"

#include <AIS_Shape.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <V3d_View.hxx>

#include <QKeyEvent>

#include <cmath>
#include <limits>

using TSA::Interaction::PickKind;

void OccView::startModelingTool(TSA::Interaction::ModelingTool* tool, const TSA::Interaction::ToolContext& ctx)
{
    if (interactionMode() == InteractionMode::ModelingTool) clearModelingToolPreview();
    m_activeTool = tool;
    m_toolCtx = ctx;
    m_toolInput.clear();
    if (!tool) return;
    tool->reset();
    setInteractionMode(InteractionMode::ModelingTool);
    setCursor(Qt::CrossCursor);
    emitModelingToolPrompt();
    if (tool->ready()) emit modelingToolReady();
}

void OccView::modelingToolApplied(bool continueTool)
{
    clearModelingToolPreview();
    if (!m_activeTool) return;
    if (continueTool)
    {
        m_activeTool->reset();
        m_toolInput.clear();
        emitModelingToolPrompt();
    }
    else
    {
        setInteractionMode(InteractionMode::Select);
    }
}

void OccView::setModelingToolSelection(const TSA::Model::ElementSet& selection)
{
    m_toolCtx.selection = selection;
}

void OccView::emitModelingToolPrompt()
{
    if (!m_activeTool) return;
    QString prompt = QString::fromStdString(m_activeTool->prompt());
    if (!m_toolInput.isEmpty()) prompt += QStringLiteral("   ›  %1").arg(m_toolInput);
    prompt += tr("   [Échap : annuler]");
    emit drawingPromptChanged(prompt);
}

void OccView::clearModelingToolPreview()
{
    clearTransformPreview();
    if (!m_toolPreviewShape.IsNull() && !m_context.IsNull())
    {
        m_context->Remove(m_toolPreviewShape, false);
        m_toolPreviewShape.Nullify();
    }
    if (!m_view.IsNull()) m_view->Redraw();
}

void OccView::handleModelingToolClick(const QPoint& p, Qt::KeyboardModifiers modifiers)
{
    if (!m_activeTool || !m_model) return;
    TSA::Interaction::ToolPick pick;
    pick.modifier = modifiers.testFlag(Qt::ControlModifier);

    const PickKind kind = m_activeTool->nextPick();
    if (kind == PickKind::None) return;
    if (kind == PickKind::Bar)
    {
        if (!pickBarAt(p, pick))
        {
            emit drawingPromptChanged(tr("Aucune barre sous le curseur — %1").arg(QString::fromStdString(m_activeTool->prompt())));
            return;
        }
    }
    else
    {
        double x = 0, y = 0, z = 0;
        int nodeId = -1;
        if (!getPointUnderCursor(p, x, y, z, nodeId)) return;
        pick.point = gp_Pnt(x, y, z);
        pick.nodeId = nodeId;
    }

    m_activeTool->addPick(pick, m_toolCtx);
    m_toolInput.clear();
    emitModelingToolPrompt();
    updateModelingToolPreview(pick.point);
    if (m_activeTool->ready()) emit modelingToolReady();
}

void OccView::updateModelingToolPreview(const gp_Pnt& cursor)
{
    if (!m_activeTool || m_context.IsNull() || m_view.IsNull()) return;
    m_toolCtx.cursor = cursor;
    const TSA::Interaction::ToolPreview preview = m_activeTool->preview(cursor, m_toolCtx);

    // Traits de construction
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    bool any = false;
    for (const auto& [a, b] : preview.lines)
    {
        if (a.Distance(b) < 1e-6) continue;
        builder.Add(compound, BRepBuilderAPI_MakeEdge(a, b).Edge());
        any = true;
    }
    if (any)
    {
        if (m_toolPreviewShape.IsNull())
        {
            m_toolPreviewShape = new AIS_Shape(compound);
            m_toolPreviewShape->SetColor(Quantity_NOC_ORANGE);
            m_toolPreviewShape->SetWidth(2.0);
            m_context->Display(m_toolPreviewShape, false);
            m_context->Deactivate(m_toolPreviewShape);
        }
        else
        {
            m_toolPreviewShape->SetShape(compound);
            m_context->Redisplay(m_toolPreviewShape, false);
        }
    }
    else if (!m_toolPreviewShape.IsNull())
    {
        m_context->Remove(m_toolPreviewShape, false);
        m_toolPreviewShape.Nullify();
    }

    // Fantômes de la sélection (construits une fois, puis transformation locale)
    if (preview.selectionTransform && m_selectionManager)
    {
        if (!m_previewGhostsBuilt) buildTransformPreviewGhosts(InteractionMode::Move3D);
        for (auto& ghost : m_previewGhostShapes)
            if (!ghost.IsNull()) ghost->SetLocalTransformation(*preview.selectionTransform);
    }
    else
    {
        clearTransformPreview();
    }
    m_view->Redraw();
}

bool OccView::handleModelingToolKey(QKeyEvent* event)
{
    if (!m_activeTool) return false;
    const int key = event->key();
    const QString text = event->text();

    if (key == Qt::Key_Escape)
    {
        if (!m_toolInput.isEmpty())
            m_toolInput.clear();
        else if (m_activeTool->pickCount() > 0)
        {
            m_activeTool->reset();
            clearModelingToolPreview();
        }
        else
        {
            setInteractionMode(InteractionMode::Select);
            return true;
        }
        emitModelingToolPrompt();
        return true;
    }
    if (key == Qt::Key_Backspace)
    {
        m_toolInput.chop(1);
        emitModelingToolPrompt();
        return true;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter)
    {
        if (!m_toolInput.isEmpty())
        {
            bool ok = false;
            const double v = QString(m_toolInput).replace(',', '.').toDouble(&ok);
            m_toolInput.clear();
            if (!ok || !m_activeTool->acceptValue(v, m_toolCtx))
            {
                emit drawingPromptChanged(tr("Valeur non utilisée à cette étape — %1").arg(QString::fromStdString(m_activeTool->prompt())));
                return true;
            }
        }
        else
        {
            m_activeTool->finish();
        }
        emitModelingToolPrompt();
        if (m_toolCtx.cursor) updateModelingToolPreview(*m_toolCtx.cursor);
        if (m_activeTool->ready()) emit modelingToolReady();
        return true;
    }
    if (text.size() == 1 && (text[0].isDigit() || text[0] == '.' || text[0] == ',' || (text[0] == '-' && m_toolInput.isEmpty())))
    {
        m_toolInput += text;
        emitModelingToolPrompt();
        return true;
    }
    return false;   // autres touches : comportement normal de la vue (F, R…)
}

bool OccView::pickBarAt(const QPoint& p, TSA::Interaction::ToolPick& pick)
{
    if (!m_model || m_view.IsNull()) return false;
    constexpr double kPixelTol = 12.0;
    double best = kPixelTol;
    bool found = false;

    auto consider = [&](TSA::Model::ElementKind kind, int id, int s, int e) {
        const auto* a = m_model->getNode(s);
        const auto* b = m_model->getNode(e);
        if (!a || !b) return;
        int ax = 0, ay = 0, bx = 0, by = 0;
        m_view->Convert(a->x(), a->y(), a->z(), ax, ay);
        m_view->Convert(b->x(), b->y(), b->z(), bx, by);
        const double dx = bx - ax, dy = by - ay;
        const double len2 = dx * dx + dy * dy;
        double t = len2 > 0 ? ((p.x() - ax) * dx + (p.y() - ay) * dy) / len2 : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        const double d = std::hypot(ax + t * dx - p.x(), ay + t * dy - p.y());
        if (d >= best) return;
        best = d;
        found = true;
        pick.bar = TSA::Interaction::BarRef { kind, id };
        pick.barParameter = t;
        pick.point = gp_Pnt(a->x() + t * (b->x() - a->x()), a->y() + t * (b->y() - a->y()), a->z() + t * (b->z() - a->z()));
    };
    using TSA::Model::ElementKind;
    for (const auto& [id, e] : m_model->beams()) consider(ElementKind::Beam, id, e.startNodeId(), e.endNodeId());
    for (const auto& [id, e] : m_model->columns()) consider(ElementKind::Column, id, e.startNodeId(), e.endNodeId());
    for (const auto& [id, e] : m_model->trussMembers()) consider(ElementKind::TrussMember, id, e.startNodeId(), e.endNodeId());
    if (!found) return false;

    // Accrochage : si le point accroché (nœud, grille, milieu…) est sur la barre, il est retenu.
    double x = 0, y = 0, z = 0;
    int nodeId = -1;
    gp_Pnt a, b;
    if (getPointUnderCursor(p, x, y, z, nodeId) && m_isCursorSnapped &&
        TSA::Interaction::ToolGeometry::barEnds(*m_model, *pick.bar, a, b))
    {
        const gp_Pnt s(x, y, z);
        const gp_Vec ab(a, b);
        const double l2 = ab.SquareMagnitude();
        if (l2 > 1e-12)
        {
            const double t = gp_Vec(a, s).Dot(ab) / l2;
            const gp_Pnt onBar = a.Translated(ab * t);
            if (t >= 0.0 && t <= 1.0 && onBar.Distance(s) < 1e-3)
            {
                pick.point = s;
                pick.barParameter = t;
                pick.nodeId = nodeId;
            }
        }
    }
    return true;
}
