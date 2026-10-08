#include "BlueprintEditor.h"

#include "BlueprintScene.h"
#include "Blueprint/BlueprintFile.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <map>

namespace TSA::UI
{

using namespace TSA::Blueprint;

namespace
{
/// Vue : molette = zoom autour du curseur, bouton du milieu = déplacement, cadre = sélection.
class BlueprintView : public QGraphicsView
{
public:
    using QGraphicsView::QGraphicsView;

protected:
    void wheelEvent(QWheelEvent* e) override
    {
        const double f = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        const double current = transform().m11();
        if ((f > 1 && current > 3.0) || (f < 1 && current < 0.2)) return;
        setTransformationAnchor(AnchorUnderMouse);
        scale(f, f);
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::MiddleButton)
        {
            m_pan = true;
            m_last = e->position().toPoint();
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        QGraphicsView::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (m_pan)
        {
            const QPoint d = e->position().toPoint() - m_last;
            m_last = e->position().toPoint();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
            return;
        }
        QGraphicsView::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (m_pan && e->button() == Qt::MiddleButton)
        {
            m_pan = false;
            unsetCursor();
            return;
        }
        QGraphicsView::mouseReleaseEvent(e);
    }

private:
    bool m_pan = false;
    QPoint m_last;
};

constexpr int kTypeRole = Qt::UserRole + 10;
} // namespace

BlueprintEditor::BlueprintEditor(QWidget* parent)
    : QWidget(parent)
    , m_library(NodeLibrary::standard())
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* bar = new QToolBar(this);
    bar->addAction(QIcon(":/icons/file_new.svg"), tr("Nouveau Blueprint"), this, &BlueprintEditor::newBlueprint);
    bar->addAction(QIcon(":/icons/file_open.svg"), tr("Ouvrir un Blueprint…"), this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Ouvrir un Blueprint"), m_file, tr("Blueprint (*.tsbp)"));
        if (!path.isEmpty()) open(path);
    });
    bar->addAction(QIcon(":/icons/file_save.svg"), tr("Enregistrer le Blueprint"), this, &BlueprintEditor::save);
    bar->addSeparator();
    bar->addAction(tr("Valider"), this, &BlueprintEditor::validate);
    QAction* runAct = bar->addAction(tr("▶  Exécuter"), this, &BlueprintEditor::run);
    runAct->setToolTip(tr("Exécuter le Blueprint sur le projet ouvert (une entrée Annuler par commande)"));
    bar->addSeparator();
    bar->addAction(tr("Supprimer"), this, [this] { m_scene->deleteSelection(); });
    root->addWidget(bar);

    auto* split = new QSplitter(Qt::Horizontal, this);
    root->addWidget(split, 1);

    // Palette de nœuds
    auto* left = new QWidget(split);
    auto* leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(4, 4, 4, 4);
    m_search = new QLineEdit(left);
    m_search->setPlaceholderText(tr("Rechercher un nœud…"));
    m_search->setClearButtonEnabled(true);
    leftLay->addWidget(m_search);
    m_palette = new QTreeWidget(left);
    m_palette->setHeaderHidden(true);
    m_palette->setToolTip(tr("Double-cliquer pour ajouter le nœud au centre de la vue"));
    leftLay->addWidget(m_palette, 1);
    connect(m_search, &QLineEdit::textChanged, this, &BlueprintEditor::filterPalette);
    connect(m_palette, &QTreeWidget::itemDoubleClicked, this, &BlueprintEditor::addFromPalette);
    buildPalette();

    // Scène
    m_scene = new BlueprintScene(m_graph, m_library, this);
    m_view = new BlueprintView(m_scene, split);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    connect(m_scene, &BlueprintScene::message, this, [this](const QString& t) { emit logMessage(t, "WARN"); });
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &BlueprintEditor::showNodeProperties);
    connect(m_scene, &BlueprintScene::graphChanged, this, [this] { m_scene->showReport(nullptr); showNodeProperties(); });

    // Valeurs du nœud sélectionné
    auto* right = new QWidget(split);
    auto* rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(8, 8, 8, 8);
    m_nodeTitle = new QLabel(right);
    m_nodeTitle->setStyleSheet("font-weight: 700; font-size: 13px;");
    m_nodeDescription = new QLabel(right);
    m_nodeDescription->setWordWrap(true);
    m_nodeDescription->setStyleSheet("color: palette(mid);");
    rightLay->addWidget(m_nodeTitle);
    rightLay->addWidget(m_nodeDescription);
    m_propertiesHost = new QWidget(right);
    m_properties = new QFormLayout(m_propertiesHost);
    rightLay->addWidget(m_propertiesHost);
    rightLay->addStretch();

    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 0);
    split->setSizes({ 240, 900, 280 });

    newBlueprint();
}

BlueprintEditor::~BlueprintEditor() = default;

void BlueprintEditor::buildPalette()
{
    m_palette->clear();
    std::map<std::string, QTreeWidgetItem*> categories;
    for (const auto* d : m_library.definitions())
    {
        auto& cat = categories[d->category];
        if (!cat)
        {
            cat = new QTreeWidgetItem(m_palette, { QString::fromStdString(d->category) });
            cat->setFlags(Qt::ItemIsEnabled);
            cat->setExpanded(true);
        }
        auto* item = new QTreeWidgetItem(cat, { QString::fromStdString(d->title) });
        item->setData(0, kTypeRole, QString::fromStdString(d->id));
        item->setToolTip(0, QStringLiteral("%1\n\n%2").arg(QString::fromStdString(d->description), QString::fromStdString(d->id)));
    }
}

void BlueprintEditor::filterPalette(const QString& text)
{
    for (int c = 0; c < m_palette->topLevelItemCount(); ++c)
    {
        auto* cat = m_palette->topLevelItem(c);
        int visible = 0;
        for (int i = 0; i < cat->childCount(); ++i)
        {
            auto* item = cat->child(i);
            const bool show = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive)
                              || item->data(0, kTypeRole).toString().contains(text, Qt::CaseInsensitive);
            item->setHidden(!show);
            visible += show ? 1 : 0;
        }
        cat->setHidden(visible == 0);
    }
}

void BlueprintEditor::addFromPalette()
{
    auto* item = m_palette->currentItem();
    if (!item || item->data(0, kTypeRole).toString().isEmpty()) return;
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    m_scene->addNode(item->data(0, kTypeRole).toString().toStdString(), center - QPointF(100, 30));
}

void BlueprintEditor::newBlueprint()
{
    m_graph.clear();
    m_graph.name = tr("Nouveau Blueprint").toStdString();
    m_graph.addNode("event.start", -300, 0);
    m_file.clear();
    m_scene->rebuild();
    m_view->centerOn(0, 0);
    showNodeProperties();
}

bool BlueprintEditor::open(const QString& path)
{
    QString error;
    Graph loaded;
    if (!loadFile(path, loaded, &error))
    {
        QMessageBox::warning(this, tr("Blueprint"), tr("Impossible d'ouvrir le Blueprint :\n%1").arg(error));
        return false;
    }
    m_graph = std::move(loaded);
    m_file = path;
    m_scene->rebuild();
    QTimer::singleShot(0, this, &BlueprintEditor::fitGraph);   // après affichage de l'onglet (taille réelle)
    emit logMessage(tr("Blueprint ouvert : %1").arg(path), "SYS");
    validate();
    return true;
}

void BlueprintEditor::fitGraph()
{
    // Tout le Blueprint s'il tient lisiblement (zoom entre 45 % et 100 %) ; sinon, début du flux (à gauche).
    const QRectF r = m_scene->itemsBoundingRect().adjusted(-40, -40, 40, 40);
    if (r.isEmpty()) return;
    const QSize vp = m_view->viewport()->size();
    const double fit = std::min(vp.width() / r.width(), vp.height() / r.height());
    const double scale = std::clamp(fit, 0.45, 1.0);
    m_view->resetTransform();
    m_view->scale(scale, scale);
    const double visibleWidth = vp.width() / scale;
    m_view->centerOn(fit >= 0.45 ? r.center() : QPointF(r.left() + visibleWidth / 2, r.center().y()));
}

void BlueprintEditor::setGraph(Graph graph)
{
    m_graph = std::move(graph);
    m_file.clear();
    m_scene->rebuild();
    QTimer::singleShot(0, this, &BlueprintEditor::fitGraph);   // après affichage de l'onglet (taille réelle)
    showNodeProperties();
    validate();
}

bool BlueprintEditor::save()
{
    if (m_file.isEmpty()) return saveAs();
    QString error;
    if (!saveFile(m_graph, m_file, &error))
    {
        QMessageBox::warning(this, tr("Blueprint"), tr("Échec de l'enregistrement :\n%1").arg(error));
        return false;
    }
    emit logMessage(tr("Blueprint enregistré : %1").arg(m_file), "SYS");
    return true;
}

bool BlueprintEditor::saveAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Enregistrer le Blueprint"),
                                                m_file.isEmpty() ? tr("Blueprint.tsbp") : m_file, tr("Blueprint (*.tsbp)"));
    if (path.isEmpty()) return false;
    if (!path.endsWith(QLatin1String(".tsbp"), Qt::CaseInsensitive)) path += QStringLiteral(".tsbp");
    m_file = path;
    return save();
}

void BlueprintEditor::validate()
{
    const auto issues = m_library.validate(m_graph);
    if (issues.empty())
    {
        emit logMessage(tr("Blueprint valide (%1 nœud(s), %2 lien(s)).").arg(m_graph.nodes().size()).arg(m_graph.links().size()), "SYS");
        return;
    }
    for (const auto& i : issues)
        emit logMessage(tr("Blueprint : %1%2").arg(i.node ? QStringLiteral("#%1 ").arg(i.node) : QString(), QString::fromStdString(i.message)),
                        "WARN");
}

bool BlueprintEditor::run()
{
    if (!m_session)
    {
        emit logMessage(tr("Aucun projet ouvert : le Blueprint ne peut pas être exécuté."), "ERROR");
        return false;
    }
    Runner runner(m_library, m_graph, m_session);
    const ExecutionReport report = runner.run();
    for (const auto& line : report.log) emit logMessage(QString::fromStdString(line), "INFO");
    emit logMessage(tr("Blueprint : %1 (%2 ms)").arg(QString::fromStdString(report.message)).arg(report.milliseconds, 0, 'f', 1),
                    report.ok ? "SYS" : "ERROR");
    m_scene->showReport(&report);
    emit projectModified();
    return report.ok;
}

void BlueprintEditor::showNodeProperties()
{
    while (m_properties->rowCount() > 0) m_properties->removeRow(0);
    const int id = m_scene->selectedNode();
    const NodeInstance* n = id ? m_graph.node(id) : nullptr;
    const NodeDefinition* d = n ? m_library.find(n->type) : nullptr;
    if (!d)
    {
        m_nodeTitle->setText(tr("Blueprint"));
        m_nodeDescription->setText(tr("Double-cliquer un nœud de la palette pour l'ajouter ; tirer d'une broche à une autre "
                                      "pour les relier ; Suppr pour effacer ; molette pour zoomer, bouton du milieu pour se déplacer."));
        return;
    }
    m_nodeTitle->setText(QStringLiteral("%1  #%2").arg(QString::fromStdString(d->title)).arg(id));
    m_nodeDescription->setText(QString::fromStdString(d->description));

    for (const auto& pin : d->inputs)
    {
        if (pin.kind != PinKind::Data || pin.any) continue;
        if (m_graph.linkTo(id, pin.name))
        {
            m_properties->addRow(QString::fromStdString(pin.label), new QLabel(tr("(relié)"), m_propertiesHost));
            continue;
        }
        const auto it = n->values.find(pin.name);
        const Value v = it != n->values.end() ? it->second : pin.defaultValue;
        const std::string pinName = pin.name;
        auto set = [this, id, pinName](const Value& value) {
            m_graph.setValue(id, pinName, value);
            m_scene->refreshNode(id);
        };
        QString label = QString::fromStdString(pin.label);
        if (const char* unit = TSA::Automation::quantityUnit(pin.quantity); unit[0]) label += QStringLiteral(" (%1)").arg(QString::fromUtf8(unit));

        QWidget* editor = nullptr;
        switch (pin.type)
        {
        case ValueType::Bool:
        {
            auto* w = new QCheckBox(m_propertiesHost);
            w->setChecked(std::holds_alternative<bool>(v) && std::get<bool>(v));
            connect(w, &QCheckBox::toggled, this, [set](bool b) { set(b); });
            editor = w;
            break;
        }
        case ValueType::Integer:
        {
            auto* w = new QSpinBox(m_propertiesHost);
            w->setRange(-1000000, 1000000);
            w->setValue(std::holds_alternative<long long>(v) ? int(std::get<long long>(v)) : 0);
            connect(w, &QSpinBox::valueChanged, this, [set](int x) { set(static_cast<long long>(x)); });
            editor = w;
            break;
        }
        case ValueType::Real:
        {
            auto* w = new QDoubleSpinBox(m_propertiesHost);
            w->setRange(-1e9, 1e9);
            w->setDecimals(4);
            w->setValue(std::holds_alternative<double>(v) ? std::get<double>(v)
                        : std::holds_alternative<long long>(v) ? double(std::get<long long>(v)) : 0.0);
            connect(w, &QDoubleSpinBox::valueChanged, this, [set](double x) { set(x); });
            editor = w;
            break;
        }
        case ValueType::Point3:
        {
            auto* w = new QWidget(m_propertiesHost);
            auto* lay = new QHBoxLayout(w);
            lay->setContentsMargins(0, 0, 0, 0);
            const Point3 p = std::holds_alternative<Point3>(v) ? std::get<Point3>(v) : Point3 { 0, 0, 0 };
            auto boxes = std::make_shared<std::array<QDoubleSpinBox*, 3>>();
            for (int k = 0; k < 3; ++k)
            {
                auto* b = new QDoubleSpinBox(w);
                b->setRange(-1e6, 1e6);
                b->setDecimals(3);
                b->setValue(p[k]);
                (*boxes)[k] = b;
                lay->addWidget(b);
            }
            for (int k = 0; k < 3; ++k)
                connect((*boxes)[k], &QDoubleSpinBox::valueChanged, this,
                        [set, boxes](double) { set(Point3 { (*boxes)[0]->value(), (*boxes)[1]->value(), (*boxes)[2]->value() }); });
            editor = w;
            break;
        }
        case ValueType::Text:
        case ValueType::IdList:
        {
            auto* w = new QLineEdit(QString::fromStdString(std::holds_alternative<std::monostate>(v) ? std::string() : toText(v)), m_propertiesHost);
            const bool ids = pin.type == ValueType::IdList;
            connect(w, &QLineEdit::editingFinished, this, [set, w, ids] {
                if (!ids)
                {
                    set(w->text().toStdString());
                    return;
                }
                std::vector<int> list;
                for (const QString& part : w->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) list.push_back(part.trimmed().toInt());
                set(list);
            });
            editor = w;
            break;
        }
        }
        if (pin.required) label += QStringLiteral(" *");
        m_properties->addRow(label, editor);
    }
}

} // namespace TSA::UI
