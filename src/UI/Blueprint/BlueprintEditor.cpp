#include "BlueprintEditor.h"

#include "BlueprintScene.h"
#include "BlueprintObjectViewer.h"
#include "Blueprint/BlueprintFile.h"
#include "Blueprint/BlueprintScript.h"
#include "Automation/CommandRegistry.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QEventLoop>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QTabWidget>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <functional>
#include <map>

namespace TSA::UI
{

using namespace TSA::Blueprint;

namespace
{
/// Vue : molette = zoom autour du curseur, bouton du milieu = déplacement, cadre = sélection. Les touches de l'éditeur
/// (copier, coller, dupliquer, commentaire, documentation) sont prises par la vue quand elle a le focus, avant les
/// raccourcis de la fenêtre (Ctrl+C / Ctrl+V de la fenêtre de TSA) : sinon le raccourci serait ambigu.
class BlueprintView : public QGraphicsView
{
public:
    using QGraphicsView::QGraphicsView;
    std::function<bool(QKeyEvent*)> handles;   ///< touche traitée par l'éditeur
    std::function<void(QKeyEvent*)> onKey;

protected:
    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::ShortcutOverride && handles && handles(static_cast<QKeyEvent*>(e)))
        {
            e->accept();
            return true;
        }
        return QGraphicsView::event(e);
    }
    void keyPressEvent(QKeyEvent* e) override
    {
        if (handles && onKey && handles(e))
        {
            onKey(e);
            e->accept();
            return;
        }
        QGraphicsView::keyPressEvent(e);
    }
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
constexpr int kNodeRole = Qt::UserRole + 11;     // valeurs produites, problèmes : nœud
constexpr int kPinRole = Qt::UserRole + 12;      // valeurs produites : sortie (vide : variable)
constexpr int kVariableRole = Qt::UserRole + 13; // valeurs produites : nom de variable
const char* const kFragmentMime = "application/x-tsaraloha-blueprint";

QString realText(double v)
{
    return QString::number(v, 'g', 12);
}

/// Texte d'une valeur sur une ligne (tableaux, rapports : première ligne suivie de « … »).
QString oneLine(const std::string& text)
{
    const QString t = QString::fromStdString(text);
    const int nl = t.indexOf(QLatin1Char('\n'));
    return nl < 0 ? t : t.left(nl) + QStringLiteral(" …");
}

/// Ligne « sortie = valeur » : valeur sur une ligne, texte complet (borné) en info-bulle.
void fillValueRow(QTreeWidgetItem* row, const std::string& text, bool object)
{
    row->setText(1, oneLine(text));
    QString tip = QString::fromStdString(text);
    if (tip.size() > 4000) tip = tip.left(4000) + QStringLiteral("\n…");
    if (object) tip += QObject::tr("\n\nDouble-clic ou Entrée : afficher l'objet");
    row->setToolTip(1, tip);
}
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
    m_actUndo = bar->addAction(QIcon(":/icons/undo.svg"), tr("Annuler (graphe)"), this, &BlueprintEditor::undo);
    m_actRedo = bar->addAction(QIcon(":/icons/redo.svg"), tr("Rétablir (graphe)"), this, &BlueprintEditor::redo);
    m_actUndo->setToolTip(tr("Annuler la dernière modification du Blueprint (l'historique du projet est distinct)"));
    bar->addAction(tr("Supprimer"), this, [this] { m_scene->deleteSelection(); });
    // Touches traitées par la vue du graphe (BlueprintView), pas par des raccourcis : voir la classe
    auto shortcutAction = [&](const QString& text, const QKeySequence& key, auto slot, const QString& tip) {
        QAction* a = bar->addAction(text, this, slot);
        a->setToolTip(QStringLiteral("%1 (%2, dans le graphe)").arg(tip, key.toString(QKeySequence::NativeText)));
        return a;
    };
    shortcutAction(tr("Copier"), QKeySequence::Copy, [this] { copySelection(); }, tr("Copier les nœuds et commentaires sélectionnés"));
    shortcutAction(tr("Coller"), QKeySequence::Paste, [this] { paste(); }, tr("Coller des nœuds copiés (liens internes conservés)"));
    shortcutAction(tr("Dupliquer"), QKeySequence(Qt::CTRL | Qt::Key_D), [this] { duplicateSelection(); }, tr("Dupliquer la sélection"));
    shortcutAction(tr("Commentaire"), QKeySequence(Qt::Key_C), [this] { addComment(); },
                   tr("Encadrer les nœuds sélectionnés d'un commentaire (titre en première ligne)"));
    shortcutAction(tr("Documentation"), QKeySequence(Qt::Key_F1), [this] { showDocumentation(); },
                   tr("Documentation, hypothèses et code source du nœud sélectionné"));
    bar->addSeparator();
    bar->addAction(tr("Valider"), this, &BlueprintEditor::validate);
    m_actRun = bar->addAction(tr("▶  Exécuter"), this, &BlueprintEditor::run);
    m_actRun->setToolTip(tr("Exécuter le Blueprint sur le projet ouvert (une entrée Annuler par commande) ; un Blueprint "
                            "sans accès au projet s'exécute en tâche de fond"));
    m_actDebug = bar->addAction(tr("Déboguer"), this, &BlueprintEditor::debug);
    m_actDebug->setToolTip(tr("Exécuter en s'arrêtant aux points d'arrêt (au premier nœud s'il n'y en a aucun)"));
    m_actStep = bar->addAction(tr("Pas à pas"), this, &BlueprintEditor::step);
    m_actContinue = bar->addAction(tr("Continuer"), this, &BlueprintEditor::continueExecution);
    m_actStop = bar->addAction(tr("■ Arrêter"), this, &BlueprintEditor::stop);
    m_actBreakpoint = bar->addAction(tr("● Point d'arrêt"), this, [this] { toggleBreakpoint(m_scene->selectedNode()); });
    m_actBreakpoint->setToolTip(tr("Ajouter / retirer un point d'arrêt sur le nœud sélectionné"));
    bar->addSeparator();
    auto* scriptMenu = new QMenu(this);
    scriptMenu->addAction(tr("Importer un script de commandes…"), this, [this] {
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(this, tr("Script de commandes"),
            tr("Une commande du registre par ligne ; « a = commande … » nomme une commande, « a.id » réutilise sa sortie :"),
            QStringLiteral("a = model.create_node position=0,0,0\nb = model.create_node position=6,0,0\n"
                           "model.create_beam start=a.id end=b.id section=\"IPE 300\""), &ok);
        if (!ok) return;
        QString error;
        if (!importScript(text, &error)) QMessageBox::warning(this, tr("Script de commandes"), error);
    });
    scriptMenu->addAction(tr("Copier en script de commandes"), this, [this] {
        QStringList warnings;
        QApplication::clipboard()->setText(exportScript(&warnings));
        emit logMessage(tr("Script de commandes copié dans le presse-papiers."), "SYS");
        for (const QString& w : warnings) emit logMessage(tr("Script : %1").arg(w), "WARN");
    });
    scriptMenu->addAction(tr("Copier la description (contexte pour l'IA)"), this, [this] {
        QApplication::clipboard()->setText(describeGraph());
        emit logMessage(tr("Description du Blueprint copiée dans le presse-papiers."), "SYS");
    });
    auto* scriptButton = new QToolButton(bar);
    scriptButton->setText(tr("Script"));
    scriptButton->setMenu(scriptMenu);
    scriptButton->setPopupMode(QToolButton::InstantPopup);
    bar->addWidget(scriptButton);
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
    auto* view = new BlueprintView(m_scene, split);
    view->handles = [](QKeyEvent* k) {
        const auto mods = k->modifiers() & ~Qt::KeypadModifier;
        return k->matches(QKeySequence::Copy) || k->matches(QKeySequence::Paste) || (k->key() == Qt::Key_D && mods == Qt::ControlModifier)
               || (k->key() == Qt::Key_C && mods == Qt::NoModifier) || (k->key() == Qt::Key_F1 && mods == Qt::NoModifier);
    };
    view->onKey = [this](QKeyEvent* k) {
        if (k->matches(QKeySequence::Copy)) copySelection();
        else if (k->matches(QKeySequence::Paste)) paste();
        else if (k->key() == Qt::Key_D) duplicateSelection();
        else if (k->key() == Qt::Key_C) addComment();
        else if (k->key() == Qt::Key_F1) showDocumentation();
    };
    m_view = view;
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    connect(m_scene, &BlueprintScene::message, this, [this](const QString& t) { emit logMessage(t, "WARN"); });
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &BlueprintEditor::showNodeProperties);
    connect(m_scene, &BlueprintScene::graphChanged, this, [this] {
        m_scene->showReport(nullptr);
        showNodeProperties();
        recordChange();
        refreshProblems();
    });
    connect(m_scene, &BlueprintScene::nodesMoved, this, [this] { recordChange(); });
    connect(m_scene, &BlueprintScene::commentEditRequested, this, &BlueprintEditor::editComment);

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
    auto* statusRow = new QHBoxLayout();
    m_nodeStatus = new QLabel(right);
    m_nodeStatus->setWordWrap(true);
    auto* docButton = new QPushButton(tr("Documentation…"), right);
    docButton->setToolTip(tr("Formulation, hypothèses, unités, limites et code source du nœud (F1)"));
    connect(docButton, &QPushButton::clicked, this, &BlueprintEditor::showDocumentation);
    m_docButton = docButton;
    statusRow->addWidget(m_nodeStatus, 1);
    statusRow->addWidget(docButton);
    rightLay->addLayout(statusRow);
    m_propertiesHost = new QWidget(right);
    m_properties = new QFormLayout(m_propertiesHost);
    rightLay->addWidget(m_propertiesHost);
    rightLay->addStretch();
    auto* tabs = new QTabWidget(right);
    m_sideTabs = tabs;
    auto* valuesPage = new QWidget(tabs);
    auto* valuesLay = new QVBoxLayout(valuesPage);
    valuesLay->setContentsMargins(0, 4, 0, 0);
    m_watchTitle = new QLabel(tr("Valeurs produites"), valuesPage);
    m_watchTitle->setWordWrap(true);
    valuesLay->addWidget(m_watchTitle);
    m_watch = new QTreeWidget(valuesPage);
    m_watch->setColumnCount(2);
    m_watch->setHeaderLabels({ tr("Nœud / sortie"), tr("Valeur") });
    m_watch->setToolTip(tr("Double-cliquer une valeur objet (matrice, vecteur, résultat…) pour l'afficher"));
    valuesLay->addWidget(m_watch, 1);
    tabs->addTab(valuesPage, tr("Valeurs"));
    m_problems = new QTreeWidget(tabs);
    m_problems->setColumnCount(2);
    m_problems->setHeaderLabels({ tr("Nœud"), tr("Problème") });
    m_problems->setRootIsDecorated(false);
    m_problems->setToolTip(tr("Erreurs bloquantes, avertissements et erreur de la dernière exécution ; double-cliquer pour aller au nœud"));
    tabs->addTab(m_problems, tr("Problèmes"));
    tabs->setMinimumHeight(220);
    rightLay->addWidget(tabs, 1);
    // Double-clic ou Entrée (itemActivated) : ouvrir la valeur, aller au nœud du problème
    connect(m_watch, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) { openValue(item); });
    connect(m_problems, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {
        if (const int node = item->data(0, kNodeRole).toInt()) m_scene->focusNode(node);
    });

    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 0);
    split->setSizes({ 240, 900, 280 });

    newBlueprint();
}

BlueprintEditor::~BlueprintEditor()
{
    // Fenêtre fermée pendant une pause : l'exécution est interrompue proprement.
    if (m_pauseLoop) m_pauseLoop->exit(2);
    if (m_runner) m_runner->requestStop();
    // La scène (enfant QObject) est détruite APRÈS ce destructeur, par ~QObject : en se vidant elle émet
    // selectionChanged, qui appellerait showNodeProperties sur un éditeur déjà détruit (assertion Qt
    // « Called object is not of the correct type » à la fermeture, BUG-039). Connexions coupées ici.
    for (QObject* child : findChildren<QObject*>())
        QObject::disconnect(child, nullptr, this, nullptr);
}

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
        QString label = QString::fromStdString(d->title);
        if (d->status == NodeStatus::Validated) label += QStringLiteral("  ✓");
        else if (d->status == NodeStatus::Experimental) label += QStringLiteral("  ⚗");
        auto* item = new QTreeWidgetItem(cat, { label });
        item->setData(0, kTypeRole, QString::fromStdString(d->id));
        item->setToolTip(0, QStringLiteral("%1\n\n%2 — composant %3").arg(QString::fromStdString(d->description), QString::fromStdString(d->id),
                                                                        QString::fromUtf8(statusName(d->status))));
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
    m_breakpoints.clear();
    m_scene->setBreakpoints(m_breakpoints);
    m_scene->rebuild();
    m_view->centerOn(0, 0);
    graphReplaced();
    showNodeProperties();
    resetHistory();
}

void BlueprintEditor::graphReplaced()
{
    m_cache.clear();
    m_lastReport = {};
    m_hasReport = false;
    m_scene->showReport(nullptr);
    refreshProblems();
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
    m_breakpoints.clear();
    m_scene->setBreakpoints(m_breakpoints);
    m_scene->rebuild();
    graphReplaced();
    resetHistory();
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
    m_breakpoints.clear();
    m_scene->setBreakpoints(m_breakpoints);
    m_scene->rebuild();
    graphReplaced();
    QTimer::singleShot(0, this, &BlueprintEditor::fitGraph);   // après affichage de l'onglet (taille réelle)
    showNodeProperties();
    resetHistory();
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
    refreshProblems();
    const auto issues = m_library.validate(m_graph);
    for (const auto& i : m_library.warnings(m_graph))
        emit logMessage(tr("Blueprint (avertissement) : %1%2").arg(i.node ? QStringLiteral("#%1 ").arg(i.node) : QString(), QString::fromStdString(i.message)),
                        "WARN");
    if (issues.empty())
    {
        emit logMessage(tr("Blueprint valide (%1 nœud(s), %2 lien(s)).").arg(m_graph.nodes().size()).arg(m_graph.links().size()), "SYS");
        return;
    }
    for (const auto& i : issues)
        emit logMessage(tr("Blueprint : %1%2").arg(i.node ? QStringLiteral("#%1 ").arg(i.node) : QString(), QString::fromStdString(i.message)),
                        "WARN");
}

void BlueprintEditor::refreshProblems()
{
    if (!m_problems) return;
    m_problems->clear();
    auto add = [this](int node, const QString& kind, const std::string& text) {
        const auto* n = node ? m_graph.node(node) : nullptr;
        const auto* d = n ? m_library.find(n->type) : nullptr;
        auto* item = new QTreeWidgetItem(m_problems, { node ? QStringLiteral("#%1 %2").arg(node).arg(d ? QString::fromStdString(d->title) : QString())
                                                            : tr("graphe"),
                                                       QStringLiteral("%1 : %2").arg(kind, QString::fromStdString(text)) });
        item->setData(0, kNodeRole, node);
        item->setToolTip(1, QString::fromStdString(text));
    };
    for (const auto& i : m_library.validate(m_graph)) add(i.node, tr("erreur"), i.message);
    for (const auto& i : m_library.warnings(m_graph)) add(i.node, tr("avertissement"), i.message);
    if (m_hasReport && !m_lastReport.ok && !m_lastReport.message.empty()) add(m_lastReport.failedNode, tr("exécution"), m_lastReport.message);
    m_problems->resizeColumnToContents(0);
}

bool BlueprintEditor::run()
{
    return execute(false, false);
}

bool BlueprintEditor::debug()
{
    return execute(true, m_breakpoints.empty());
}

bool BlueprintEditor::execute(bool debugging, bool stepFromStart)
{
    if (m_running) return false;
    if (!m_session)
    {
        emit logMessage(tr("Aucun projet ouvert : le Blueprint ne peut pas être exécuté."), "ERROR");
        return false;
    }
    ExecutionReport report;
    const bool background = !debugging && !m_library.usesProject(m_graph);
    setRunning(true);
    if (background)
    {
        // Aucun accès au projet : exécution dans un thread de travail sur une copie du graphe ; la boucle
        // locale garde l'interface réactive (Arrêter reste utilisable).
        const Graph copy = m_graph;
        Runner runner(m_library, copy, nullptr);
        runner.setCache(&m_cache);   // l'interface ne touche pas au cache pendant l'exécution (édition bloquée)
        m_runner = &runner;
        QEventLoop loop;
        QThread* worker = QThread::create([&runner, &report] { report = runner.run(); });
        connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
        worker->start();
        loop.exec();
        worker->wait();
        delete worker;
        m_runner = nullptr;
        emit logMessage(tr("Blueprint exécuté en tâche de fond (aucun accès au projet)."), "INFO");
    }
    else
    {
        Runner runner(m_library, m_graph, m_session);
        runner.setCache(&m_cache);
        BreakpointDebugger debugger([this](const DebugState& s) { return pause(s); }, m_breakpoints, stepFromStart);
        if (debugging)
        {
            runner.setDebugger(&debugger);
            m_debugger = &debugger;
        }
        m_runner = &runner;
        report = runner.run();
        m_runner = nullptr;
        m_debugger = nullptr;
    }
    m_scene->setActiveNode(0);
    m_lastReport = report;
    m_hasReport = true;
    m_pauseValues.clear();
    setRunning(false);
    for (const auto& line : report.log) emit logMessage(QString::fromStdString(line), "INFO");
    emit logMessage(tr("Blueprint : %1 (%2 ms)").arg(QString::fromStdString(report.message)).arg(report.milliseconds, 0, 'f', 1),
                    report.ok ? "SYS" : "ERROR");
    m_scene->showReport(&m_lastReport);
    refreshProblems();
    showValues();
    // Erreur : onglet « Problèmes » et nœud fautif centré ; réussite : valeurs produites
    if (!report.ok)
    {
        m_sideTabs->setCurrentWidget(m_problems);
        if (report.failedNode) m_scene->focusNode(report.failedNode);
    }
    else
        m_sideTabs->setCurrentIndex(0);
    if (!background) emit projectModified();
    return report.ok;
}

DebugAction BlueprintEditor::pause(const DebugState& state)
{
    m_scene->setActiveNode(state.node);
    m_pauseValues = state.outputs ? *state.outputs : std::map<int, std::map<std::string, Value>> {};
    showWatch(&m_pauseValues);
    const NodeInstance* n = m_graph.node(state.node);
    const NodeDefinition* d = n ? m_library.find(n->type) : nullptr;
    emit logMessage(tr("Débogueur : pause avant « %1 » (#%2), %3 étape(s) exécutée(s).")
                        .arg(d ? QString::fromStdString(d->title) : QStringLiteral("?")).arg(state.node).arg(state.step),
                    "INFO");
    emit projectModified();   // vues à jour pendant la pause (commandes déjà exécutées)
    QEventLoop loop;
    m_pauseLoop = &loop;
    updateActions();
    const int code = loop.exec();
    m_pauseLoop = nullptr;
    updateActions();
    return code == 1 ? DebugAction::Step : code == 2 ? DebugAction::Abort : DebugAction::Continue;
}

void BlueprintEditor::showWatch(const std::map<int, std::map<std::string, Value>>* outputs)
{
    m_watch->clear();
    if (m_watchTitle) m_watchTitle->setText(tr("Valeurs produites jusqu'à la pause (débogueur)"));
    if (!outputs) return;
    for (const auto& [id, pins] : *outputs)
    {
        const NodeInstance* n = m_graph.node(id);
        const NodeDefinition* d = n ? m_library.find(n->type) : nullptr;
        auto* item = new QTreeWidgetItem(m_watch, { QStringLiteral("#%1 %2").arg(id).arg(d ? QString::fromStdString(d->title) : QString()) });
        for (const auto& [pin, v] : pins)
        {
            auto* row = new QTreeWidgetItem(item, { QString::fromStdString(pin) });
            fillValueRow(row, toText(v), std::holds_alternative<ObjectRef>(v));
            row->setData(0, kNodeRole, id);
            row->setData(0, kPinRole, QString::fromStdString(pin));
        }
        item->setExpanded(true);
    }
    m_watch->resizeColumnToContents(0);
}

void BlueprintEditor::showValues()
{
    if (!m_watch || m_pauseLoop) return;
    m_watch->clear();
    if (!m_hasReport)
    {
        m_watchTitle->setText(tr("Valeurs produites : exécuter le Blueprint pour les voir"));
        return;
    }
    const int selected = m_scene->selectedNode();
    m_watchTitle->setText(selected ? tr("Valeurs produites par le nœud #%1 (dernière exécution) ; double-clic : afficher").arg(selected)
                                   : tr("Valeurs produites par la dernière exécution (%1 nœud(s), %2 réutilisation(s)) ; double-clic : afficher")
                                         .arg(m_lastReport.values.size())
                                         .arg(m_lastReport.reused));
    for (const auto& [id, pins] : m_lastReport.values)
    {
        if (selected && id != selected) continue;
        const NodeInstance* n = m_graph.node(id);
        const NodeDefinition* d = n ? m_library.find(n->type) : nullptr;
        auto* item = new QTreeWidgetItem(m_watch, { QStringLiteral("#%1 %2").arg(id).arg(d ? QString::fromStdString(d->title) : QString()) });
        for (const auto& [pin, v] : pins)
        {
            const PinSpec* spec = d ? d->output(pin) : nullptr;
            auto* row = new QTreeWidgetItem(item, { spec ? QString::fromStdString(spec->label) : QString::fromStdString(pin) });
            fillValueRow(row, toText(v), std::holds_alternative<ObjectRef>(v));
            row->setData(0, kNodeRole, id);
            row->setData(0, kPinRole, QString::fromStdString(pin));
        }
        item->setExpanded(true);
    }
    if (!selected && !m_lastReport.variables.empty())
    {
        auto* vars = new QTreeWidgetItem(m_watch, { tr("Variables") });
        for (const auto& [name, v] : m_lastReport.variables)
        {
            auto* row = new QTreeWidgetItem(vars, { QString::fromStdString(name) });
            fillValueRow(row, toText(v), std::holds_alternative<ObjectRef>(v));
            row->setData(0, kVariableRole, QString::fromStdString(name));
        }
        vars->setExpanded(true);
    }
    m_watch->resizeColumnToContents(0);
}

void BlueprintEditor::openValue(QTreeWidgetItem* item)
{
    if (!item) return;
    Value v;
    const QString variable = item->data(0, kVariableRole).toString();
    if (!variable.isEmpty())
    {
        const auto it = m_lastReport.variables.find(variable.toStdString());
        if (it != m_lastReport.variables.end()) v = it->second;
    }
    else
    {
        const int node = item->data(0, kNodeRole).toInt();
        const std::string pin = item->data(0, kPinRole).toString().toStdString();
        const auto& source = m_pauseLoop ? m_pauseValues : m_lastReport.values;
        const auto it = source.find(node);
        if (it == source.end() || !it->second.count(pin)) return;
        v = it->second.at(pin);
    }
    if (const auto* o = std::get_if<ObjectRef>(&v); o && *o)
        showObjectViewer(*o, item->text(0), this);
}

void BlueprintEditor::continueExecution()
{
    if (m_pauseLoop) m_pauseLoop->exit(0);
}

void BlueprintEditor::step()
{
    if (m_pauseLoop) m_pauseLoop->exit(1);
    else if (!m_running) execute(true, true);
}

void BlueprintEditor::stop()
{
    if (m_pauseLoop) m_pauseLoop->exit(2);
    else if (m_runner) m_runner->requestStop();
}

int BlueprintEditor::selectedNode() const
{
    return m_scene->selectedNode();
}

void BlueprintEditor::toggleBreakpoint(int node)
{
    if (!node || !m_graph.node(node)) return;
    if (!m_breakpoints.erase(node)) m_breakpoints.insert(node);
    m_scene->setBreakpoints(m_breakpoints);
    if (m_debugger) m_debugger->setBreakpoints(m_breakpoints);
}

void BlueprintEditor::setRunning(bool running)
{
    m_running = running;
    m_scene->setEditable(!running);
    m_palette->setEnabled(!running);
    m_propertiesHost->setEnabled(!running);
    if (running) m_watch->clear();
    updateActions();
}

void BlueprintEditor::updateActions()
{
    if (!m_actRun) return;
    const bool paused = m_pauseLoop != nullptr;
    m_actRun->setEnabled(!m_running);
    m_actDebug->setEnabled(!m_running);
    m_actStep->setEnabled(!m_running || paused);
    m_actContinue->setEnabled(paused);
    m_actStop->setEnabled(m_running);
    m_actUndo->setEnabled(!m_running && canUndo());
    m_actRedo->setEnabled(!m_running && canRedo());
}

void BlueprintEditor::recordChange(const QString& coalesceKey)
{
    // Valeur modifiée en continu (saisie, roulette) : une seule entrée par champ tant qu'on ne change pas d'action.
    if (!coalesceKey.isEmpty() && coalesceKey == m_lastKey)
    {
        m_snapshot = m_graph;
        return;
    }
    m_lastKey = coalesceKey;
    m_undo.push_back(m_snapshot);
    if (m_undo.size() > 200) m_undo.erase(m_undo.begin());
    m_redo.clear();
    m_snapshot = m_graph;
    updateActions();
}

void BlueprintEditor::resetHistory()
{
    m_undo.clear();
    m_redo.clear();
    m_snapshot = m_graph;
    m_lastKey.clear();
    updateActions();
}

void BlueprintEditor::undo()
{
    if (m_running || m_undo.empty()) return;
    m_redo.push_back(m_graph);
    m_graph = std::move(m_undo.back());
    m_undo.pop_back();
    m_snapshot = m_graph;
    m_lastKey.clear();
    m_scene->rebuild();
    m_scene->showReport(nullptr);
    showNodeProperties();
    updateActions();
}

void BlueprintEditor::redo()
{
    if (m_running || m_redo.empty()) return;
    m_undo.push_back(m_graph);
    m_graph = std::move(m_redo.back());
    m_redo.pop_back();
    m_snapshot = m_graph;
    m_lastKey.clear();
    m_scene->rebuild();
    m_scene->showReport(nullptr);
    showNodeProperties();
    updateActions();
}

bool BlueprintEditor::importScript(const QString& script, QString* error)
{
    Graph g;
    std::string why;
    if (!fromCommandScript(script.toStdString(), m_library, TSA::Automation::CommandRegistry::builtIn(), g, &why))
    {
        if (error) *error = tr("Script invalide : %1").arg(QString::fromStdString(why));
        return false;
    }
    setGraph(std::move(g));
    emit logMessage(tr("Blueprint créé depuis un script de commandes (%1 nœud(s)).").arg(m_graph.nodes().size()), "SYS");
    return true;
}

QString BlueprintEditor::exportScript(QStringList* warnings) const
{
    std::vector<std::string> w;
    const QString s = QString::fromStdString(toCommandScript(m_graph, m_library, &w));
    if (warnings)
        for (const auto& x : w) warnings->append(QString::fromStdString(x));
    return s;
}

QString BlueprintEditor::describeGraph() const
{
    return QString::fromStdString(describe(m_graph, m_library));
}

void BlueprintEditor::copySelection()
{
    const auto nodes = m_scene->selectedNodes();
    const auto comments = m_scene->selectedComments();
    if (nodes.empty() && comments.empty()) return;
    const QByteArray json = fragmentToJson(m_graph, nodes, comments);
    auto* mime = new QMimeData;
    mime->setData(QString::fromLatin1(kFragmentMime), json);
    mime->setText(QString::fromUtf8(json));
    QApplication::clipboard()->setMimeData(mime);
    emit logMessage(tr("Copié : %1 nœud(s), %2 commentaire(s).").arg(nodes.size()).arg(comments.size()), "SYS");
}

void BlueprintEditor::paste()
{
    if (m_running) return;
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime) return;
    QByteArray json = mime->data(QString::fromLatin1(kFragmentMime));
    if (json.isEmpty()) json = mime->text().toUtf8();
    std::vector<int> nodes, comments;
    QString error;
    if (!pasteFragment(m_graph, json, 40.0, 40.0, &nodes, &comments, &error))
    {
        emit logMessage(tr("Coller : %1").arg(error), "WARN");
        return;
    }
    m_scene->rebuild();
    m_scene->selectItems(nodes, comments);
    recordChange();
    refreshProblems();
    showNodeProperties();
}

void BlueprintEditor::duplicateSelection()
{
    if (m_running) return;
    const auto selectedNodes = m_scene->selectedNodes();
    const auto selectedComments = m_scene->selectedComments();
    if (selectedNodes.empty() && selectedComments.empty()) return;
    std::vector<int> nodes, comments;
    if (!pasteFragment(m_graph, fragmentToJson(m_graph, selectedNodes, selectedComments), 40.0, 40.0, &nodes, &comments)) return;
    m_scene->rebuild();
    m_scene->selectItems(nodes, comments);
    recordChange();
    refreshProblems();
    showNodeProperties();
}

int BlueprintEditor::addComment(const QString& text)
{
    if (m_running) return 0;
    QRectF frame;
    for (int id : m_scene->selectedNodes())
        if (const auto* n = m_graph.node(id)) frame = frame.united(QRectF(n->x, n->y, 200.0, 120.0));
    if (frame.isNull())
    {
        const QPointF c = m_view->mapToScene(m_view->viewport()->rect().center());
        frame = QRectF(c.x() - 200.0, c.y() - 120.0, 400.0, 240.0);
    }
    else
        frame.adjust(-30.0, -50.0, 30.0, 30.0);
    const int id = m_scene->addComment(frame, text.isEmpty() ? tr("Commentaire") : text);
    if (text.isEmpty()) editComment(id);
    return id;
}

void BlueprintEditor::editComment(int id)
{
    auto* c = m_graph.comment(id);
    if (!c || m_running) return;
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Commentaire"), tr("Titre (première ligne) et texte :"),
                                                        QString::fromStdString(c->text), &ok);
    if (!ok) return;
    c->text = text.toStdString();
    m_scene->refreshComment(id);
    recordChange();
}

void BlueprintEditor::showDocumentation()
{
    const int id = m_scene->selectedNode();
    const NodeInstance* n = id ? m_graph.node(id) : nullptr;
    const NodeDefinition* d = n ? m_library.find(n->type) : nullptr;
    if (!d)
    {
        // Nœud choisi dans la palette
        auto* item = m_palette->currentItem();
        if (item && !item->data(0, kTypeRole).toString().isEmpty()) d = m_library.find(item->data(0, kTypeRole).toString().toStdString());
    }
    if (d) showNodeDocumentation(*d, this);
}

void BlueprintEditor::showNodeProperties()
{
    while (m_properties->rowCount() > 0) m_properties->removeRow(0);
    const int id = m_scene->selectedNode();
    const NodeInstance* n = id ? m_graph.node(id) : nullptr;
    const NodeDefinition* d = n ? m_library.find(n->type) : nullptr;
    showValues();
    if (!d)
    {
        m_nodeTitle->setText(tr("Blueprint"));
        m_nodeDescription->setText(tr("Double-cliquer un nœud de la palette pour l'ajouter ; tirer d'une broche à une autre "
                                      "pour les relier ; Suppr pour effacer ; C pour encadrer d'un commentaire ; Ctrl+C / Ctrl+V pour "
                                      "copier / coller ; F1 pour la documentation ; molette pour zoomer, bouton du milieu pour se déplacer."));
        m_nodeStatus->clear();
        m_docButton->setEnabled(false);
        return;
    }
    m_nodeTitle->setText(QStringLiteral("%1  #%2").arg(QString::fromStdString(d->title)).arg(id));
    m_nodeDescription->setText(QString::fromStdString(d->description));
    m_docButton->setEnabled(true);
    switch (d->status)
    {
    case NodeStatus::Validated: m_nodeStatus->setText(tr("✓ composant validé (testé contre des références)")); break;
    case NodeStatus::Experimental: m_nodeStatus->setText(tr("⚗ composant expérimental (non validé)")); break;
    case NodeStatus::Standard: m_nodeStatus->setText(d->cacheable ? tr("résultat réutilisé tant que ses entrées ne changent pas") : QString()); break;
    }

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
            recordChange(QStringLiteral("%1:%2").arg(id).arg(QString::fromStdString(pinName)));
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
            // Pleine précision et notation scientifique (Iz = 8.356e-5 m⁴, E = 2.1e8 kN/m²) ; virgule ou point décimal
            const double current = std::holds_alternative<double>(v) ? std::get<double>(v)
                                   : std::holds_alternative<long long>(v) ? double(std::get<long long>(v)) : 0.0;
            auto* w = new QLineEdit(realText(current), m_propertiesHost);
            w->setToolTip(tr("Réel : notation scientifique admise (2.1e8), virgule ou point décimal"));
            connect(w, &QLineEdit::editingFinished, this, [this, set, w, current] {
                bool ok = false;
                const double x = QLocale::c().toDouble(w->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')), &ok);
                if (ok) set(x);
                else
                {
                    emit logMessage(tr("Valeur réelle invalide : « %1 »").arg(w->text()), "WARN");
                    w->setText(realText(current));
                }
            });
            editor = w;
            break;
        }
        case ValueType::Object:
            editor = new QLabel(tr("objet %1 : relier une sortie").arg(QString::fromStdString(pinTypeName(pin))), m_propertiesHost);
            break;
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
