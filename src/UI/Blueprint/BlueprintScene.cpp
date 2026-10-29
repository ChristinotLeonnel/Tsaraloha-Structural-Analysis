#include "BlueprintScene.h"

#include "Blueprint/BlueprintSubgraph.h"

#include <QGraphicsView>

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

#include <algorithm>

namespace TSA::UI
{

using namespace TSA::Blueprint;

namespace
{
constexpr double kWidth = 200.0;
constexpr double kHeader = 24.0;
constexpr double kRow = 20.0;
constexpr double kPinRadius = 5.0;
constexpr int kLinkData = Qt::UserRole + 1;   // index du lien dans le graphe

QColor categoryColor(const std::string& category)
{
    // Programmation structurelle de TSALab (bibliothèque A à E) et composants expérimentaux
    auto starts = [&](const char* p) { return category.rfind(p, 0) == 0; };
    if (starts("Structure A")) return QColor("#00796B");
    if (starts("Structure B")) return QColor("#0277BD");
    if (starts("Structure C")) return QColor("#3949AB");
    if (starts("Structure D")) return QColor("#558B2F");
    if (starts("Structure E")) return QColor("#5D4037");
    if (starts("Expérimental")) return QColor("#E65100");
    if (category == "Variables") return QColor("#8E24AA");
    if (category == "Événements") return QColor("#B3261E");
    if (category == "Paramètres") return QColor("#5E35B1");
    if (category == "Math") return QColor("#2E7D32");
    if (category == "Logique") return QColor("#AD1457");
    if (category == "Flux") return QColor("#455A64");
    if (category == "Texte" || category == "Débogage") return QColor("#6D4C41");
    if (category == "TSALab") return QColor("#7C4DFF");
    if (category == "Requêtes") return QColor("#00838F");
    return QColor("#1565C0");   // commandes du projet (Modèle, Charges…)
}

QPainterPath bezier(const QPointF& a, const QPointF& b)
{
    QPainterPath p(a);
    const double dx = std::max(40.0, std::abs(b.x() - a.x()) * 0.5);
    p.cubicTo(a + QPointF(dx, 0), b - QPointF(dx, 0), b);
    return p;
}
} // namespace

QColor pinColor(const PinSpec& pin)
{
    if (pin.kind == PinKind::Exec) return QColor("#ECEFF1");
    if (pin.any) return QColor("#9E9E9E");
    switch (pin.type)
    {
    case ValueType::Bool: return QColor("#E53935");
    case ValueType::Integer: return QColor("#26C6DA");
    case ValueType::Real: return QColor("#9CCC65");
    case ValueType::Text: return QColor("#F06292");
    case ValueType::Point3: return QColor("#FFD54F");
    case ValueType::IdList: return QColor("#FFA726");
    case ValueType::Object:
    {
        static const std::map<std::string, QColor> known = {
            { "Matrix", QColor("#BA68C8") },         { "Vector", QColor("#9575CD") },          { "StructuralModel", QColor("#4DB6AC") },
            { "Node2D", QColor("#81C784") },         { "FrameElement2D", QColor("#4FC3F7") }, { "Material", QColor("#BCAAA4") },
            { "Section2D", QColor("#B0BEC5") },      { "SupportCondition", QColor("#FF8A65") }, { "Load", QColor("#FFB74D") },
            { "LoadCase", QColor("#FFCC80") },       { "LoadCombination", QColor("#FFE082") }, { "DOFSet", QColor("#F48FB1") },
            { "ReducedSystem", QColor("#CE93D8") },  { "SolverResult", QColor("#80CBC4") },   { "InternalForceDiagram", QColor("#E6EE9C") },
        };
        if (const auto it = known.find(pin.objectType); it != known.end()) return it->second;
        const std::size_t h = std::hash<std::string> {}(pin.objectType);
        return QColor::fromHsv(static_cast<int>(h % 360), 120, 230);
    }
    }
    return Qt::gray;
}

// -----------------------------------------------------------------------------
// Élément graphique d'un nœud
// -----------------------------------------------------------------------------

class BlueprintNodeItem : public QGraphicsItem
{
public:
    BlueprintNodeItem(BlueprintScene& scene, int id) : m_scene(scene), m_id(id)
    {
        const auto* n = scene.graph().node(id);
        setPos(n->x, n->y);   // avant ItemSendsGeometryChanges : pas de notification à la construction
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setZValue(1);
    }

    int id() const { return m_id; }
    const NodeDefinition* definition() const
    {
        const auto* n = m_scene.graph().node(m_id);
        return n ? m_scene.library().find(n->type) : nullptr;
    }

    int rows() const
    {
        const auto* d = definition();
        return d ? static_cast<int>(std::max(d->inputs.size(), d->outputs.size())) : 1;
    }

    QRectF boundingRect() const override { return QRectF(-8, -8, kWidth + 16, kHeader + rows() * kRow + 30); }

    QPointF pinLocal(const std::string& pin, bool output) const
    {
        const auto* d = definition();
        if (!d) return {};
        const auto& list = output ? d->outputs : d->inputs;
        for (std::size_t i = 0; i < list.size(); ++i)
            if (list[i].name == pin) return QPointF(output ? kWidth : 0.0, kHeader + kRow * (i + 0.5));
        return {};
    }

    void setStats(const NodeStats* stats, bool failed)
    {
        m_hasStats = stats != nullptr;
        if (stats) m_stats = *stats;
        m_failed = failed;
        update();
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const auto* n = m_scene.graph().node(m_id);
        const auto* d = definition();
        const double h = kHeader + rows() * kRow + 6;
        p->setRenderHint(QPainter::Antialiasing);
        const bool active = m_scene.activeNode() == m_id;
        QPen border(active ? QColor("#00E5FF") : m_failed ? QColor("#FF5252") : isSelected() ? QColor("#FFC107") : QColor("#11151A"),
                    active ? 3.5 : m_failed || isSelected() ? 2.5 : 1.0);
        p->setPen(border);
        p->setBrush(QColor(38, 42, 49, 240));
        p->drawRoundedRect(QRectF(0, 0, kWidth, h), 6, 6);

        QPainterPath header;
        header.addRoundedRect(QRectF(0, 0, kWidth, kHeader), 6, 6);
        const bool instance = !subgraphOfType(n->type).empty();
        const bool bodyEnd = !instance && n->type.rfind("sub.", 0) == 0;   // entrée / sortie d'un corps de sous-graphe
        p->fillPath(header, !d ? QColor("#B71C1C") : instance ? QColor("#00695C") : bodyEnd ? QColor("#37474F") : categoryColor(d->category));
        p->setPen(Qt::white);
        QFont f = p->font();
        f.setBold(true);
        f.setPointSizeF(8.5);
        p->setFont(f);
        QString title = d ? QString::fromStdString(displayText(d->title)) : BlueprintScene::tr("Inconnu : %1").arg(QString::fromStdString(n->type));
        if (d && d->status == NodeStatus::Validated) title = QStringLiteral("✓ ") + title;
        else if (d && d->status == NodeStatus::Experimental) title = QStringLiteral("⚗ ") + title;
        if (d && instance) title = QStringLiteral("⧉ ") + title;
        p->drawText(QRectF(8, 0, kWidth - 24, kHeader), Qt::AlignVCenter | Qt::AlignLeft,
                    p->fontMetrics().elidedText(title, Qt::ElideRight, int(kWidth - 24)));
        if (m_scene.hasBreakpoint(m_id))
        {
            p->setPen(QPen(Qt::white, 1.0));
            p->setBrush(QColor("#E53935"));
            p->drawEllipse(QPointF(kWidth - 10, kHeader / 2), 5, 5);
        }
        if (!d) return;

        f.setBold(false);
        f.setPointSizeF(7.5);
        p->setFont(f);
        auto drawPin = [&](const PinSpec& pin, const QPointF& c, bool connected) {
            p->setPen(QPen(pinColor(pin), 1.5));
            p->setBrush(connected ? pinColor(pin) : Qt::NoBrush);
            if (pin.kind == PinKind::Exec)
            {
                QPolygonF tri { c + QPointF(-5, -6), c + QPointF(5, 0), c + QPointF(-5, 6) };
                p->drawPolygon(tri);
            }
            else
                p->drawEllipse(c, kPinRadius, kPinRadius);
        };
        for (std::size_t i = 0; i < d->inputs.size(); ++i)
        {
            const auto& pin = d->inputs[i];
            const QPointF c(0.0, kHeader + kRow * (i + 0.5));
            const bool linked = m_scene.graph().linkTo(m_id, pin.name).has_value();
            drawPin(pin, c, linked);
            QString label = QString::fromStdString(displayText(pin.label));
            if (pin.kind == PinKind::Data && !linked)
            {
                const auto it = n->values.find(pin.name);
                const Value v = (it != n->values.end()) ? it->second : pin.defaultValue;
                if (!std::holds_alternative<std::monostate>(v)) label += QStringLiteral(" = ") + QString::fromStdString(toText(v));
                else if (pin.required) label += BlueprintScene::tr(" (requis)");
            }
            p->setPen(QColor("#CFD8DC"));
            p->drawText(QRectF(10, c.y() - kRow / 2, kWidth / 2 + 30, kRow), Qt::AlignVCenter | Qt::AlignLeft,
                        p->fontMetrics().elidedText(label, Qt::ElideRight, int(kWidth / 2 + 30)));
        }
        for (std::size_t i = 0; i < d->outputs.size(); ++i)
        {
            const auto& pin = d->outputs[i];
            const QPointF c(kWidth, kHeader + kRow * (i + 0.5));
            drawPin(pin, c, !m_scene.graph().linksFrom(m_id, pin.name).empty());
            p->setPen(QColor("#CFD8DC"));
            p->drawText(QRectF(kWidth / 2, c.y() - kRow / 2, kWidth / 2 - 10, kRow), Qt::AlignVCenter | Qt::AlignRight,
                        QString::fromStdString(displayText(pin.label)));
        }
        if (m_hasStats)
        {
            p->setPen(QColor("#FFD54F"));
            QString line = QStringLiteral("× %1   %2 ms").arg(m_stats.executions).arg(m_stats.milliseconds, 0, 'f', 2);
            if (m_stats.reused > 0) line += QStringLiteral("   ↺ %1").arg(m_stats.reused);
            p->drawText(QRectF(0, h + 2, kWidth, 14), Qt::AlignLeft, line);
        }
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == ItemPositionHasChanged) m_scene.nodeMoved(m_id, value.toPointF());
        return QGraphicsItem::itemChange(change, value);
    }
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override
    {
        m_scene.requestNodeOpen(m_id);
        e->accept();
    }

private:
    BlueprintScene& m_scene;
    int m_id = 0;
    bool m_hasStats = false;
    bool m_failed = false;
    NodeStats m_stats;
};

// -----------------------------------------------------------------------------
// Commentaire (cadre titré)
// -----------------------------------------------------------------------------

class BlueprintCommentItem : public QGraphicsItem
{
public:
    static constexpr double kGrip = 14.0;

    BlueprintCommentItem(BlueprintScene& scene, int id) : m_scene(scene), m_id(id)
    {
        const auto* c = scene.graph().comment(id);
        setPos(c->x, c->y);
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setZValue(-1);
        setAcceptHoverEvents(true);
        m_last = pos();
    }

    int id() const { return m_id; }
    /// Taille ou texte modifiés hors de la souris (éditeur).
    void refresh()
    {
        prepareGeometryChange();
        update();
    }

    QRectF boundingRect() const override
    {
        const auto* c = m_scene.graph().comment(m_id);
        return c ? QRectF(-2, -2, c->width + 4, c->height + 4) : QRectF();
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const auto* c = m_scene.graph().comment(m_id);
        if (!c) return;
        const QColor color(QString::fromStdString(c->color));
        QColor fill = color;
        fill.setAlpha(46);
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(QPen(isSelected() ? QColor("#FFC107") : color, isSelected() ? 2.0 : 1.2));
        p->setBrush(fill);
        p->drawRoundedRect(QRectF(0, 0, c->width, c->height), 6, 6);
        const QString text = QString::fromStdString(c->text);
        const QString title = text.section(QLatin1Char('\n'), 0, 0);
        const QString body = text.section(QLatin1Char('\n'), 1);
        QPainterPath bar;
        bar.addRoundedRect(QRectF(0, 0, c->width, 26), 6, 6);
        QColor barColor = color;
        barColor.setAlpha(170);
        p->fillPath(bar, barColor);
        QFont f = p->font();
        f.setBold(true);
        f.setPointSizeF(9.5);
        p->setFont(f);
        p->setPen(Qt::white);
        p->drawText(QRectF(8, 0, c->width - 16, 26), Qt::AlignVCenter | Qt::AlignLeft,
                    p->fontMetrics().elidedText(title, Qt::ElideRight, int(c->width - 16)));
        if (!body.isEmpty())
        {
            f.setBold(false);
            f.setPointSizeF(8.0);
            p->setFont(f);
            p->setPen(QColor("#ECEFF1"));
            p->drawText(QRectF(8, 30, c->width - 16, c->height - 36), Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, body);
        }
        p->setPen(QPen(color, 1.5));
        for (int k = 1; k <= 3; ++k)
            p->drawLine(QPointF(c->width - 4 * k, c->height - 2), QPointF(c->width - 2, c->height - 4 * k));
    }

protected:
    bool onGrip(const QPointF& local) const
    {
        const auto* c = m_scene.graph().comment(m_id);
        return c && local.x() >= c->width - kGrip && local.y() >= c->height - kGrip;
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* e) override
    {
        setCursor(onGrip(e->pos()) ? Qt::SizeFDiagCursor : Qt::ArrowCursor);
        QGraphicsItem::hoverMoveEvent(e);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* e) override
    {
        m_resizing = m_scene.isEditable() && e->button() == Qt::LeftButton && onGrip(e->pos());
        m_carried.clear();
        if (!m_resizing)
            for (int n : m_scene.graph().nodesInComment(m_id)) m_carried.push_back(n);
        m_last = pos();
        m_pressAt = pos();
        QGraphicsItem::mousePressEvent(e);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override
    {
        if (m_resizing)
        {
            if (auto* c = m_scene.graph().comment(m_id))
            {
                prepareGeometryChange();
                c->width = std::max(160.0, e->pos().x());
                c->height = std::max(80.0, e->pos().y());
                update();
            }
            return;
        }
        QGraphicsItem::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override
    {
        const bool changed = m_resizing || pos() != m_pressAt;
        m_resizing = false;
        QGraphicsItem::mouseReleaseEvent(e);
        if (changed) m_scene.commentChanged();
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override
    {
        m_scene.requestCommentEdit(m_id);
        e->accept();
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == ItemPositionHasChanged)
        {
            const QPointF now = value.toPointF();
            const QPointF delta = now - m_last;
            m_last = now;
            if (auto* c = m_scene.graph().comment(m_id))
            {
                c->x = now.x();
                c->y = now.y();
            }
            // Les nœuds contenus suivent le cadre (sauf ceux déjà déplacés avec la sélection)
            for (int n : m_carried) m_scene.moveNodeBy(n, delta);
        }
        return QGraphicsItem::itemChange(change, value);
    }

private:
    BlueprintScene& m_scene;
    int m_id = 0;
    bool m_resizing = false;
    QPointF m_last, m_pressAt;
    std::vector<int> m_carried;
};

// -----------------------------------------------------------------------------
// Scène
// -----------------------------------------------------------------------------

BlueprintScene::BlueprintScene(Graph& graph, const NodeLibrary& library, QObject* parent)
    : QGraphicsScene(parent)
    , m_graph(graph)
    , m_library(library)
{
    setBackgroundBrush(QColor("#1B1E23"));
    setSceneRect(-4000, -4000, 8000, 8000);
    rebuild();
}

void BlueprintScene::rebuild()
{
    m_dragLine = nullptr;
    clear();
    m_items.clear();
    m_commentItems.clear();
    m_links.clear();
    for (const auto& [id, c] : m_graph.comments())
    {
        auto* item = new BlueprintCommentItem(*this, id);
        item->setFlag(QGraphicsItem::ItemIsMovable, m_editable);
        addItem(item);
        m_commentItems[id] = item;
    }
    for (const auto& [id, n] : m_graph.nodes())
    {
        auto* item = new BlueprintNodeItem(*this, id);
        item->setFlag(QGraphicsItem::ItemIsMovable, m_editable);
        addItem(item);
        m_items[id] = item;
    }
    rebuildLinks();
    fitSceneRectToItems();
    emit rebuilt();
}

void BlueprintScene::fitSceneRectToItems()
{
    setSceneRect(QRectF(-4000, -4000, 8000, 8000).united(itemsBoundingRect().adjusted(-2000, -2000, 2000, 2000)));
}

void BlueprintScene::rebuildLinks()
{
    for (auto* l : m_links)
    {
        removeItem(l);
        delete l;
    }
    m_links.clear();
    const auto& links = m_graph.links();
    for (std::size_t i = 0; i < links.size(); ++i)
    {
        const auto& l = links[i];
        const auto* from = m_graph.node(l.fromNode);
        const auto* def = from ? m_library.find(from->type) : nullptr;
        const auto* pin = def ? def->output(l.fromPin) : nullptr;
        auto* item = addPath(bezier(pinPosition(l.fromNode, l.fromPin, true), pinPosition(l.toNode, l.toPin, false)),
                             QPen(pin ? pinColor(*pin) : QColor(Qt::red), pin && pin->kind == PinKind::Exec ? 2.6 : 2.0));
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setData(kLinkData, static_cast<int>(i));
        item->setZValue(0);
        m_links.push_back(item);
    }
    for (auto& [id, item] : m_items) item->update();
}

QPointF BlueprintScene::pinPosition(int node, const std::string& pin, bool output) const
{
    const auto it = m_items.find(node);
    return it == m_items.end() ? QPointF() : it->second->mapToScene(it->second->pinLocal(pin, output));
}

int BlueprintScene::addNode(const std::string& type, const QPointF& pos)
{
    const int id = m_graph.addNode(type, pos.x(), pos.y());
    auto* item = new BlueprintNodeItem(*this, id);
    addItem(item);
    m_items[id] = item;
    clearSelection();
    item->setSelected(true);
    emit graphChanged();
    return id;
}

void BlueprintScene::setBreakpoints(const std::set<int>& breakpoints)
{
    m_breakpoints = breakpoints;
    for (auto& [id, item] : m_items) item->update();
}

void BlueprintScene::setActiveNode(int id)
{
    m_activeNode = id;
    for (auto& [nid, item] : m_items) item->update();
    if (const auto it = m_items.find(id); it != m_items.end())
        for (QGraphicsView* v : views()) v->ensureVisible(it->second, 80, 80);
}

void BlueprintScene::setEditable(bool editable)
{
    m_editable = editable;
    for (auto& [id, item] : m_items) item->setFlag(QGraphicsItem::ItemIsMovable, editable);
    for (auto& [id, item] : m_commentItems) item->setFlag(QGraphicsItem::ItemIsMovable, editable);
}

int BlueprintScene::addComment(const QRectF& rect, const QString& text)
{
    TSA::Blueprint::Comment c;
    c.x = rect.x(), c.y = rect.y(), c.width = std::max(160.0, rect.width()), c.height = std::max(80.0, rect.height());
    c.text = text.toStdString();
    const int id = m_graph.addComment(c);
    auto* item = new BlueprintCommentItem(*this, id);
    item->setFlag(QGraphicsItem::ItemIsMovable, m_editable);
    addItem(item);
    m_commentItems[id] = item;
    clearSelection();
    item->setSelected(true);
    emit graphChanged();
    return id;
}

void BlueprintScene::refreshComment(int id)
{
    if (const auto it = m_commentItems.find(id); it != m_commentItems.end()) it->second->refresh();
}

std::set<int> BlueprintScene::selectedNodes() const
{
    std::set<int> out;
    for (auto* item : selectedItems())
        if (auto* n = dynamic_cast<BlueprintNodeItem*>(item)) out.insert(n->id());
    return out;
}

std::set<int> BlueprintScene::selectedComments() const
{
    std::set<int> out;
    for (auto* item : selectedItems())
        if (auto* c = dynamic_cast<BlueprintCommentItem*>(item)) out.insert(c->id());
    return out;
}

void BlueprintScene::selectItems(const std::vector<int>& nodes, const std::vector<int>& comments)
{
    clearSelection();
    for (int id : nodes)
        if (const auto it = m_items.find(id); it != m_items.end()) it->second->setSelected(true);
    for (int id : comments)
        if (const auto it = m_commentItems.find(id); it != m_commentItems.end()) it->second->setSelected(true);
}

void BlueprintScene::focusNode(int id)
{
    const auto it = m_items.find(id);
    if (it == m_items.end()) return;
    clearSelection();
    it->second->setSelected(true);
    for (QGraphicsView* v : views()) v->centerOn(it->second);
}

void BlueprintScene::moveNodeBy(int id, const QPointF& delta)
{
    const auto it = m_items.find(id);
    if (it == m_items.end() || it->second->isSelected()) return;   // déjà déplacé avec la sélection
    it->second->moveBy(delta.x(), delta.y());
}

void BlueprintScene::commentChanged()
{
    emit graphChanged();
}

void BlueprintScene::nodeMoved(int id, const QPointF& pos)
{
    m_moved = true;
    if (auto* n = m_graph.node(id))
    {
        n->x = pos.x();
        n->y = pos.y();
    }
    rebuildLinks();
}

void BlueprintScene::refreshNode(int id)
{
    if (const auto it = m_items.find(id); it != m_items.end()) it->second->update();
}

int BlueprintScene::selectedNode() const
{
    int found = 0;
    for (auto* item : selectedItems())
        if (auto* n = dynamic_cast<BlueprintNodeItem*>(item))
        {
            if (found) return 0;
            found = n->id();
        }
    return found;
}

void BlueprintScene::deleteSelection()
{
    std::vector<int> nodes, comments;
    std::vector<Link> links;
    for (auto* item : selectedItems())
    {
        if (auto* n = dynamic_cast<BlueprintNodeItem*>(item)) nodes.push_back(n->id());
        else if (auto* c = dynamic_cast<BlueprintCommentItem*>(item)) comments.push_back(c->id());
        else if (item->data(kLinkData).isValid())
        {
            const int i = item->data(kLinkData).toInt();
            if (i >= 0 && i < static_cast<int>(m_graph.links().size())) links.push_back(m_graph.links()[std::size_t(i)]);
        }
    }
    if (nodes.empty() && links.empty() && comments.empty()) return;
    for (const auto& l : links) m_graph.removeLink(l);
    for (int id : nodes) m_graph.removeNode(id);
    for (int id : comments) m_graph.removeComment(id);
    rebuild();
    emit graphChanged();
}

void BlueprintScene::showReport(const ExecutionReport* report)
{
    for (auto& [id, item] : m_items)
    {
        const NodeStats* st = nullptr;
        if (report)
            if (const auto it = report->profile.find(id); it != report->profile.end()) st = &it->second;
        item->setStats(st, report && report->failedNode == id);
    }
}

bool BlueprintScene::pinAt(const QPointF& scenePos, PinHit& hit) const
{
    for (const auto& [id, item] : m_items)
    {
        const auto* def = item->definition();
        if (!def) continue;
        for (int side = 0; side < 2; ++side)
        {
            const auto& list = side ? def->outputs : def->inputs;
            for (const auto& pin : list)
            {
                const QPointF c = item->mapToScene(item->pinLocal(pin.name, side == 1));
                if (QLineF(c, scenePos).length() <= kPinRadius + 5)
                {
                    hit = { id, pin.name, side == 1 };
                    return true;
                }
            }
        }
    }
    return false;
}

void BlueprintScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_moved = false;
    PinHit hit;
    if (m_editable && event->button() == Qt::LeftButton && pinAt(event->scenePos(), hit))
    {
        m_dragFrom = hit;
        m_dragLine = addPath(QPainterPath(), QPen(QColor("#FFC107"), 2, Qt::DashLine));
        m_dragLine->setZValue(3);
        event->accept();
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}

void BlueprintScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragLine)
    {
        const QPointF a = pinPosition(m_dragFrom.node, m_dragFrom.pin, m_dragFrom.output);
        m_dragLine->setPath(m_dragFrom.output ? bezier(a, event->scenePos()) : bezier(event->scenePos(), a));
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void BlueprintScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragLine)
    {
        removeItem(m_dragLine);
        delete m_dragLine;
        m_dragLine = nullptr;
        PinHit to;
        if (pinAt(event->scenePos(), to) && to.output != m_dragFrom.output)
        {
            const PinHit& out = m_dragFrom.output ? m_dragFrom : to;
            const PinHit& in = m_dragFrom.output ? to : m_dragFrom;
            std::string why;
            if (m_library.connect(m_graph, { out.node, out.pin, in.node, in.pin }, &why))
            {
                rebuildLinks();
                emit graphChanged();
            }
            else
                emit message(tr("Lien refusé : %1").arg(QString::fromStdString(why)));
        }
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
    if (m_moved)
    {
        m_moved = false;
        emit nodesMoved();
    }
}

void BlueprintScene::keyPressEvent(QKeyEvent* event)
{
    if (m_editable && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace))
    {
        deleteSelection();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

} // namespace TSA::UI
