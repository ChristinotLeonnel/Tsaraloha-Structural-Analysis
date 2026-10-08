#include "BlueprintScene.h"

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
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
        QPen border(m_failed ? QColor("#FF5252") : isSelected() ? QColor("#FFC107") : QColor("#11151A"), m_failed || isSelected() ? 2.5 : 1.0);
        p->setPen(border);
        p->setBrush(QColor(38, 42, 49, 240));
        p->drawRoundedRect(QRectF(0, 0, kWidth, h), 6, 6);

        QPainterPath header;
        header.addRoundedRect(QRectF(0, 0, kWidth, kHeader), 6, 6);
        p->fillPath(header, d ? categoryColor(d->category) : QColor("#B71C1C"));
        p->setPen(Qt::white);
        QFont f = p->font();
        f.setBold(true);
        f.setPointSizeF(8.5);
        p->setFont(f);
        p->drawText(QRectF(8, 0, kWidth - 16, kHeader), Qt::AlignVCenter | Qt::AlignLeft,
                    d ? QString::fromStdString(d->title) : QStringLiteral("Inconnu : %1").arg(QString::fromStdString(n->type)));
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
            QString label = QString::fromStdString(pin.label);
            if (pin.kind == PinKind::Data && !linked)
            {
                const auto it = n->values.find(pin.name);
                const Value v = (it != n->values.end()) ? it->second : pin.defaultValue;
                if (!std::holds_alternative<std::monostate>(v)) label += QStringLiteral(" = ") + QString::fromStdString(toText(v));
                else if (pin.required) label += QStringLiteral(" (requis)");
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
                        QString::fromStdString(pin.label));
        }
        if (m_hasStats)
        {
            p->setPen(QColor("#FFD54F"));
            p->drawText(QRectF(0, h + 2, kWidth, 14), Qt::AlignLeft,
                        QStringLiteral("× %1   %2 ms").arg(m_stats.executions).arg(m_stats.milliseconds, 0, 'f', 2));
        }
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == ItemPositionHasChanged) m_scene.nodeMoved(m_id, value.toPointF());
        return QGraphicsItem::itemChange(change, value);
    }

private:
    BlueprintScene& m_scene;
    int m_id = 0;
    bool m_hasStats = false;
    bool m_failed = false;
    NodeStats m_stats;
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
    m_links.clear();
    for (const auto& [id, n] : m_graph.nodes())
    {
        auto* item = new BlueprintNodeItem(*this, id);
        addItem(item);
        m_items[id] = item;
    }
    rebuildLinks();
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

void BlueprintScene::nodeMoved(int id, const QPointF& pos)
{
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
    std::vector<int> nodes;
    std::vector<Link> links;
    for (auto* item : selectedItems())
    {
        if (auto* n = dynamic_cast<BlueprintNodeItem*>(item)) nodes.push_back(n->id());
        else if (item->data(kLinkData).isValid())
        {
            const int i = item->data(kLinkData).toInt();
            if (i >= 0 && i < static_cast<int>(m_graph.links().size())) links.push_back(m_graph.links()[std::size_t(i)]);
        }
    }
    if (nodes.empty() && links.empty()) return;
    for (const auto& l : links) m_graph.removeLink(l);
    for (int id : nodes) m_graph.removeNode(id);
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
    PinHit hit;
    if (event->button() == Qt::LeftButton && pinAt(event->scenePos(), hit))
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
                emit message(QStringLiteral("Lien refusé : %1").arg(QString::fromStdString(why)));
        }
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void BlueprintScene::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        deleteSelection();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

} // namespace TSA::UI
