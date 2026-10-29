// Affichage du maillage dans la vue 3D de TSA (docs/MESH_VISUALIZATION.md) : modes d'affichage, filtres,
// isolation de groupes, coupe, étiquettes, sélection de nœuds et d'éléments avec leurs propriétés, champs de
// résultats (déplacements des barres), déformée et animation. La vue, la caméra, l'accrochage et la sélection du
// modèle restent ceux d'OccView ; le maillage n'est qu'un calque (TSA::Viewer::MeshVisual).

#include "MainWindow.h"

#include "../Analysis/ResultsModel.h"
#include "../Mesh/MeshResultField.h"
#include "../Viewer/MeshVisual.h"
#include "../Viewer/OccView.h"
#include "Dock/LogConsoleDock.h"
#include "Mesh/MeshController.h"

#include <QActionGroup>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <optional>

using namespace TSA::Mesh;
using TSA::Viewer::MeshViewOptions;

namespace
{
QAction* checkable(QObject* parent, const QString& text, const QString& tip, bool checked)
{
    auto* a = new QAction(text, parent);
    a->setCheckable(true);
    a->setChecked(checked);
    a->setToolTip(tip);
    return a;
}

constexpr OccView::ElementCategory kGeometryCategories[] = { OccView::ElementCategory::Beams,  OccView::ElementCategory::Columns,
                                                             OccView::ElementCategory::Slabs,  OccView::ElementCategory::Walls,
                                                             OccView::ElementCategory::Foundations, OccView::ElementCategory::Trusses,
                                                             OccView::ElementCategory::Cables };
} // namespace

void MainWindow::createMeshViewActions()
{
    auto* styles = new QActionGroup(this);
    m_actionMeshWire = checkable(this, tr("Filaire"), tr("Arêtes uniques du maillage, sans faces"), false);
    m_actionMeshFacesEdges = checkable(this, tr("Faces et arêtes"), tr("Faces des éléments et arêtes uniques"), true);
    m_actionMeshFaces = checkable(this, tr("Faces seules"), tr("Faces des éléments sans arêtes"), false);
    m_actionMeshShrink = checkable(this, tr("Éléments réduits"), tr("Chaque élément réduit autour de son centre (contrôle de la connectivité)"), false);
    for (QAction* a : { m_actionMeshWire, m_actionMeshFacesEdges, m_actionMeshFaces, m_actionMeshShrink })
    {
        styles->addAction(a);
        connect(a, &QAction::triggered, this, &MainWindow::applyMeshViewOptions);
    }
    m_actionMeshEdges = checkable(this, tr("Arêtes du maillage"), tr("Affiche ou masque les arêtes (faces et arêtes / faces seules)"), true);
    connect(m_actionMeshEdges, &QAction::triggered, this, [this](bool on) {
        if (on && m_actionMeshFaces->isChecked()) m_actionMeshFacesEdges->setChecked(true);
        if (!on && (m_actionMeshFacesEdges->isChecked() || m_actionMeshWire->isChecked())) m_actionMeshFaces->setChecked(true);
        applyMeshViewOptions();
    });
    m_actionMeshTransparent = checkable(this, tr("Faces transparentes"), tr("Faces du maillage semi-transparentes (50 %)"), false);
    m_actionMeshOnly = checkable(this, tr("Maillage seul (masquer la géométrie)"), tr("Masque barres, dalles, voiles, semelles et nœuds du modèle ; le maillage reste affiché"), false);
    connect(m_actionMeshOnly, &QAction::toggled, this, &MainWindow::setMeshOnly);
    m_actionMeshNodes = checkable(this, tr("Nœuds du maillage"), tr("Affiche les nœuds du maillage"), false);
    m_actionMeshNodeLabels = checkable(this, tr("Numéros des nœuds"), tr("Identifiants des nœuds du maillage (au plus 1000, répartis)"), false);
    m_actionMeshElementLabels = checkable(this, tr("Numéros des éléments"), tr("Identifiants des éléments du maillage (au plus 1000, répartis)"), false);
    m_actionMesh1D = checkable(this, tr("Éléments 1D"), tr("Barres maillées (lignes)"), true);
    m_actionMesh2D = checkable(this, tr("Éléments 2D"), tr("Dalles et voiles maillés (triangles, quadrangles)"), true);
    m_actionMesh3D = checkable(this, tr("Éléments 3D"), tr("Semelles maillées (tétraèdres, hexaèdres)"), true);
    m_actionMeshInterior = checkable(this, tr("Faces intérieures des volumes"), tr("Affiche aussi les faces partagées entre volumes (inspection) ; sinon faces extérieures seules"), false);
    for (QAction* a : { m_actionMeshTransparent, m_actionMeshNodes, m_actionMeshNodeLabels, m_actionMeshElementLabels, m_actionMesh1D, m_actionMesh2D,
                        m_actionMesh3D, m_actionMeshInterior })
        connect(a, &QAction::triggered, this, &MainWindow::applyMeshViewOptions);

    m_actionMeshNodeSize = new QAction(tr("Taille des nœuds..."), this);
    connect(m_actionMeshNodeSize, &QAction::triggered, this, [this]() {
        if (!m_meshVisual) return;
        bool ok = false;
        auto o = m_meshVisual->options();
        const double v = QInputDialog::getDouble(this, tr("Taille des nœuds"), tr("Échelle des marqueurs de nœuds :"), o.nodeSize, 0.5, 10.0, 1, &ok);
        if (!ok) return;
        o.nodeSize = v;
        if (!m_actionMeshNodes->isChecked()) m_actionMeshNodes->setChecked(true);
        o.showNodes = true;
        m_meshVisual->setOptions(o);
        refreshMeshInspector();
    });
    m_actionMeshLineWidth = new QAction(tr("Épaisseur des traits..."), this);
    connect(m_actionMeshLineWidth, &QAction::triggered, this, [this]() {
        if (!m_meshVisual) return;
        bool ok = false;
        auto o = m_meshVisual->options();
        const double v = QInputDialog::getDouble(this, tr("Épaisseur des traits"), tr("Épaisseur des arêtes et éléments 1D (pixels) :"), o.lineWidth, 0.5, 10.0, 1, &ok);
        if (!ok) return;
        o.lineWidth = v;
        m_meshVisual->setOptions(o);
        refreshMeshInspector();
    });

    m_actionMeshCut = checkable(this, tr("Coupe du maillage..."), tr("Masque les éléments situés d'un côté d'un plan : les faces des volumes ainsi exposées sont affichées"), false);
    connect(m_actionMeshCut, &QAction::triggered, this, [this](bool on) {
        if (!m_meshVisual) return;
        auto o = m_meshVisual->options();
        if (on)
        {
            const QStringList axes { tr("X (plan YZ)"), tr("Y (plan XZ)"), tr("Z (plan XY)") };
            bool ok = false;
            const QString axis = QInputDialog::getItem(this, tr("Coupe du maillage"), tr("Normale du plan de coupe :"), axes, 2, false, &ok);
            if (!ok)
            {
                m_actionMeshCut->setChecked(false);
                return;
            }
            const int k = int(axes.indexOf(axis));
            double lo = 0.0, hi = 0.0;
            if (const FeMesh* mesh = m_meshVisual->mesh(); mesh && !mesh->nodes.empty())
            {
                lo = hi = k == 0 ? mesh->nodes.front().x : (k == 1 ? mesh->nodes.front().y : mesh->nodes.front().z);
                for (const auto& n : mesh->nodes)
                {
                    const double c = k == 0 ? n.x : (k == 1 ? n.y : n.z);
                    lo = std::min(lo, c);
                    hi = std::max(hi, c);
                }
            }
            const double pos = QInputDialog::getDouble(this, tr("Coupe du maillage"), tr("Position du plan (m) — éléments au-delà masqués :"), 0.5 * (lo + hi),
                                                       lo - 1e3, hi + 1e3, 3, &ok);
            if (!ok)
            {
                m_actionMeshCut->setChecked(false);
                return;
            }
            o.filter.cut = true;
            o.filter.cutNormal = { k == 0 ? 1.0 : 0.0, k == 1 ? 1.0 : 0.0, k == 2 ? 1.0 : 0.0 };
            o.filter.cutOffset = pos;
        }
        else
            o.filter.cut = false;
        m_meshVisual->setOptions(o);
        refreshMeshInspector();
    });

    m_actionMeshIsolate = new QAction(tr("Isoler des groupes..."), this);
    m_actionMeshIsolate->setToolTip(tr("N'affiche que les éléments des groupes choisis (objets maillés, groupes physiques Gmsh)"));
    connect(m_actionMeshIsolate, &QAction::triggered, this, [this]() {
        if (!m_meshVisual || !m_meshVisual->mesh()) return;
        const FeMesh& mesh = *m_meshVisual->mesh();
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Isoler des groupes du maillage"));
        auto* layout = new QVBoxLayout(&dlg);
        layout->addWidget(new QLabel(tr("Groupes affichés (aucun coché : tous) :"), &dlg));
        auto* list = new QListWidget(&dlg);
        list->setObjectName(QStringLiteral("MeshGroupList"));
        const auto current = m_meshVisual->options().filter.isolatedGroups;
        for (const auto& g : mesh.groups)
        {
            if (g.elements.empty()) continue;
            auto* item = new QListWidgetItem(tr("%1 — %2D, %3 élément(s)").arg(QString::fromStdString(g.name)).arg(g.dimension).arg(g.elements.size()), list);
            item->setData(Qt::UserRole, QString::fromStdString(g.name));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(current.count(g.name) ? Qt::Checked : Qt::Unchecked);
        }
        layout->addWidget(list);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        layout->addWidget(buttons);
        if (dlg.exec() != QDialog::Accepted) return;
        auto o = m_meshVisual->options();
        o.filter.isolatedGroups.clear();
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked) o.filter.isolatedGroups.insert(list->item(i)->data(Qt::UserRole).toString().toStdString());
        m_meshVisual->setOptions(o);
        refreshMeshInspector();
    });
    m_actionMeshAllGroups = new QAction(tr("Afficher tous les groupes"), this);
    connect(m_actionMeshAllGroups, &QAction::triggered, this, [this]() {
        if (!m_meshVisual) return;
        auto o = m_meshVisual->options();
        o.filter.isolatedGroups.clear();
        m_meshVisual->setOptions(o);
        refreshMeshInspector();
    });

    m_actionMeshPick = checkable(this, tr("Sélection dans le maillage"),
                                 tr("Clic sur un nœud ou un élément du maillage : propriétés dans « Inspection du maillage »"), false);
    connect(m_actionMeshPick, &QAction::toggled, this, [this](bool on) {
        if (m_meshVisual) m_meshVisual->setPickMode(on);
        if (on && m_meshInspectDock)
        {
            m_meshInspectDock->show();
            m_meshInspectDock->raise();
        }
        if (!on && m_meshVisual) m_meshVisual->clearHighlight();
        if (m_occView && !m_occView->view().IsNull()) m_occView->view()->Redraw();
    });
    m_actionMeshFit = new QAction(tr("Cadrer sur le maillage"), this);
    connect(m_actionMeshFit, &QAction::triggered, this, [this]() {
        if (m_meshVisual && m_occView) m_occView->fitBox(m_meshVisual->bounds());
    });

    m_actionMeshResults = checkable(this, tr("Déplacements sur le maillage"),
                                    tr("Déplacements calculés reportés sur le maillage (couleurs, légende, déformée) ; nœuds sans résultat en gris"), false);
    connect(m_actionMeshResults, &QAction::toggled, this, [this](bool on) {
        if (!on && m_actionMeshAnimate) m_actionMeshAnimate->setChecked(false);
        refreshMeshOverlay();
    });
    m_actionMeshDeformScale = new QAction(tr("Facteur de déformation..."), this);
    connect(m_actionMeshDeformScale, &QAction::triggered, this, [this]() {
        bool ok = false;
        const double v = QInputDialog::getDouble(this, tr("Facteur de déformation"), tr("Facteur d'amplification des déplacements (0 : automatique) :"),
                                                 m_meshDeformScale, 0.0, 1e9, 2, &ok);
        if (!ok) return;
        m_meshDeformScale = v;
        refreshMeshOverlay();
    });
    m_actionMeshAnimate = checkable(this, tr("Animer la déformée"), tr("Animation de la déformée du maillage (facteur oscillant)"), false);
    connect(m_actionMeshAnimate, &QAction::toggled, this, [this](bool on) {
        if (!m_meshAnimTimer) return;
        if (on && (!m_meshVisual || !m_meshVisual->hasResults()))
        {
            m_actionMeshAnimate->setChecked(false);
            if (m_statusInfo) m_statusInfo->setText(tr("Animation : activer d'abord « Déplacements sur le maillage » avec des résultats calculés"));
            return;
        }
        m_meshAnimPhase = 0.0;
        if (on) m_meshAnimTimer->start(50);
        else
        {
            m_meshAnimTimer->stop();
            if (m_meshVisual && m_meshVisual->hasResults()) m_meshVisual->setDeformationScale(m_meshDeformScale > 0 ? m_meshDeformScale : m_meshAutoScale);
        }
    });
}

void MainWindow::populateMeshViewMenu(QMenu* menu)
{
    menu->addActions({ m_actionMeshWire, m_actionMeshFacesEdges, m_actionMeshFaces, m_actionMeshShrink });
    menu->addSeparator();
    menu->addActions({ m_actionMeshEdges, m_actionMeshNodes, m_actionMeshTransparent, m_actionMeshOnly });
    menu->addActions({ m_actionMeshNodeLabels, m_actionMeshElementLabels, m_actionMeshNodeSize, m_actionMeshLineWidth });
    menu->addSeparator();
    menu->addActions({ m_actionMesh1D, m_actionMesh2D, m_actionMesh3D, m_actionMeshInterior, m_actionMeshCut });
    menu->addActions({ m_actionMeshIsolate, m_actionMeshAllGroups });
    menu->addSeparator();
    menu->addActions({ m_actionMeshPick, m_actionMeshFit });
    menu->addSeparator();
    menu->addActions({ m_actionMeshResults, m_actionMeshDeformScale, m_actionMeshAnimate });
    // Grandeur colorée : déplacement (nœuds) ou résultantes des coques (éléments) ; la déformée reste celle des déplacements
    QMenu* fieldMenu = menu->addMenu(tr("Grandeur affichée sur le maillage"));
    auto* group = new QActionGroup(fieldMenu);
    // Noms des champs = clés de MeshResultSet::find (inchangées) ; libellé du menu traduit à l'affichage
    for (const char* name : { QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Déplacement"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Moment principal (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Effort principal dans le plan (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Mxx (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Myy (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Mxy (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Nxx (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Nyy (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Nxy (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Vxz (coque)"),
                              QT_TRANSLATE_NOOP("TSA::Mesh::MeshResultField", "Vyz (coque)") })
    {
        QAction* a = fieldMenu->addAction(QCoreApplication::translate("TSA::Mesh::MeshResultField", name));
        a->setCheckable(true);
        a->setChecked(m_meshFieldName == QString::fromUtf8(name));
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, name] {
            m_meshFieldName = QString::fromUtf8(name);
            if (m_actionMeshResults && !m_actionMeshResults->isChecked()) m_actionMeshResults->setChecked(true);
            else refreshMeshOverlay();
        });
    }
}

void MainWindow::setupMeshView()
{
    m_meshInspectText = new QPlainTextEdit(this);
    m_meshInspectText->setObjectName(QStringLiteral("MeshInspectText"));
    m_meshInspectText->setReadOnly(true);
    m_meshInspectText->setAccessibleName(tr("Inspection du maillage"));
    m_meshInspectDock = new QDockWidget(tr("Inspection du maillage"), this);
    m_meshInspectDock->setObjectName(QStringLiteral("MeshInspectDock"));
    m_meshInspectDock->setWidget(m_meshInspectText);
    addDockWidget(Qt::RightDockWidgetArea, m_meshInspectDock);
    m_meshInspectDock->hide();
    m_meshAnimTimer = new QTimer(this);
    connect(m_meshAnimTimer, &QTimer::timeout, this, [this]() {
        if (!m_meshVisual || !m_meshVisual->hasResults())
        {
            m_actionMeshAnimate->setChecked(false);
            return;
        }
        m_meshAnimPhase += 0.2;
        const double base = m_meshDeformScale > 0 ? m_meshDeformScale : m_meshAutoScale;
        m_meshVisual->setDeformationScale(base * std::sin(m_meshAnimPhase));
        // Grand maillage : la cadence suit la durée de construction (jamais d'accumulation d'images en retard).
        m_meshAnimTimer->setInterval(std::max(50, int(m_meshVisual->lastBuildMs() * 1.5)));
    });
    if (m_occView)
        m_occView->setOverlayPickHandlers(
            [this](const Handle(SelectMgr_EntityOwner)& owner) {
                TSA::Viewer::MeshVisual::Pick pick;
                if (!m_meshVisual || !m_meshVisual->handlePick(owner, &pick)) return false;
                m_meshPickText = QString::fromStdString(pick.text);
                if (m_statusInfo) m_statusInfo->setText(m_meshPickText.section('\n', 0, 0));
                if (m_consoleDock) m_consoleDock->appendLog(tr("Maillage — sélection : %1").arg(m_meshPickText.section('\n', 0, 0)), "INFO");
                refreshMeshInspector();
                return true;
            },
            [this](const Handle(SelectMgr_EntityOwner)& owner) { return m_meshVisual ? QString::fromStdString(m_meshVisual->hoverText(owner)) : QString(); });
    connect(this, &MainWindow::resultsChanged, this, [this]() {
        if (m_actionMeshResults && m_actionMeshResults->isChecked()) refreshMeshOverlay();
    });
}

void MainWindow::applyMeshViewOptions()
{
    if (!m_meshVisual) return;
    MeshViewOptions o = m_meshVisual->options();
    o.style = m_actionMeshWire->isChecked()     ? MeshViewOptions::Style::Wireframe
              : m_actionMeshFaces->isChecked()  ? MeshViewOptions::Style::Faces
              : m_actionMeshShrink->isChecked() ? MeshViewOptions::Style::Shrink
                                                : MeshViewOptions::Style::FacesEdges;
    m_actionMeshEdges->setChecked(o.style == MeshViewOptions::Style::Wireframe || o.style == MeshViewOptions::Style::FacesEdges);
    o.transparency = m_actionMeshTransparent->isChecked() ? 0.5 : 0.0;
    o.showNodes = m_actionMeshNodes->isChecked();
    o.nodeLabels = m_actionMeshNodeLabels->isChecked();
    o.elementLabels = m_actionMeshElementLabels->isChecked();
    o.filter.show1D = m_actionMesh1D->isChecked();
    o.filter.show2D = m_actionMesh2D->isChecked();
    o.filter.show3D = m_actionMesh3D->isChecked();
    o.filter.volumeExteriorOnly = !m_actionMeshInterior->isChecked();
    m_meshVisual->setOptions(o);
    refreshMeshInspector();
}

void MainWindow::setMeshOnly(bool on)
{
    if (!m_occView) return;
    if (on)
    {
        m_meshOnlySavedCategories.clear();
        for (auto c : kGeometryCategories)
        {
            m_meshOnlySavedCategories.push_back(m_occView->isElementCategoryVisible(c));
            m_occView->setElementCategoryVisible(c, false);
        }
        m_meshOnlySavedNodes = m_occView->areNodesVisible();
        m_occView->setNodesVisible(false);
        if (m_actionMeshShow && !m_actionMeshShow->isChecked()) m_actionMeshShow->setChecked(true);
    }
    else if (!m_meshOnlySavedCategories.empty())
    {
        std::size_t i = 0;
        for (auto c : kGeometryCategories) m_occView->setElementCategoryVisible(c, m_meshOnlySavedCategories[i++]);
        m_occView->setNodesVisible(m_meshOnlySavedNodes);
        m_meshOnlySavedCategories.clear();
    }
    // Vue 3D jamais affichée (démarrage en mode Blueprint) : pas encore de V3d_View
    if (!m_occView->view().IsNull()) m_occView->view()->Redraw();
}

void MainWindow::refreshMeshOverlay()
{
    if (!m_meshVisual || !m_meshController) return;
    if (const FeMesh* preview = m_meshController->preview())
    {
        m_meshVisual->show(*preview, TSA::Viewer::MeshVisual::Mode::Preview);
        refreshMeshInspector();
        return;
    }
    if (m_actionMeshShow && m_actionMeshShow->isChecked() && m_meshController->hasMesh())
    {
        const FeMesh& mesh = m_meshController->project().mesh;
        const auto& q = m_meshController->quality().perElementQuality;
        // Résultats : uniquement des valeurs calculées par le solveur pour le modèle courant (aucune extrapolation).
        std::optional<MeshResultField> displacement, colorField;
        if (m_actionMeshResults && m_actionMeshResults->isChecked())
        {
            if (!m_resultsModel || !m_resultsModel->isValid() || !m_model)
            {
                if (m_statusInfo) m_statusInfo->setText(tr("Déplacements sur le maillage : aucun résultat valide (lancer le calcul)"));
            }
            else
            {
                const auto& rec = m_meshController->project().record;
                const MeshResultSet set = ResultsModelMeshAdapter(*m_model, *m_resultsModel, rec.generated + "|" + rec.settingsHash).extract(mesh);
                if (const auto* f = set.find("Déplacement")) displacement = *f;
                if (const auto* f = set.find(m_meshFieldName.toStdString()); f && f != set.find("Déplacement")) colorField = *f;
                else if (m_meshFieldName != QStringLiteral("Déplacement") && m_statusInfo)
                    m_statusInfo->setText(tr("« %1 » : aucune valeur calculée sur ce maillage (coques non calculées ou maillage différent du calcul)").arg(QCoreApplication::translate("TSA::Mesh::MeshResultField", m_meshFieldName.toUtf8().constData())));
                if (m_consoleDock)
                {
                    m_consoleDock->appendLog(tr("Résultats sur le maillage : %1").arg(displacement ? QString::fromStdString(displacement->method) : tr("aucune valeur")), "INFO");
                    for (const auto& n : set.notes) m_consoleDock->appendLog(QString::fromStdString(n), "WARN");
                }
            }
        }
        m_meshAutoScale = 0.0;
        if (displacement)
        {
            // Facteur automatique : plus grand déplacement affiché à 5 % de la dimension du maillage.
            double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
            for (const auto& n : mesh.nodes)
                for (int k = 0; k < 3; ++k)
                {
                    const double c = k == 0 ? n.x : (k == 1 ? n.y : n.z);
                    lo[k] = std::min(lo[k], c);
                    hi[k] = std::max(hi[k], c);
                }
            const double size = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
            const double umax = displacement->range().second;
            m_meshAutoScale = umax > 1e-15 ? 0.05 * size / umax : 1.0;
        }
        const double scale = m_meshDeformScale > 0 ? m_meshDeformScale : m_meshAutoScale;
        const MeshResultField* color = colorField ? &*colorField : (displacement ? &*displacement : nullptr);
        m_meshVisual->show(mesh, TSA::Viewer::MeshVisual::Mode::Generated, &q, 0.1, color, displacement ? &*displacement : nullptr, scale);
        if (!m_meshVisual->model().issues().empty() && m_consoleDock)
            m_consoleDock->appendLog(tr("Affichage du maillage : %1 élément(s) écarté(s) (voir « Inspection du maillage »)").arg(m_meshVisual->model().issues().size()), "WARN");
    }
    else
    {
        m_meshVisual->clear();
        if (m_actionMeshOnly && m_actionMeshOnly->isChecked()) m_actionMeshOnly->setChecked(false);
    }
    m_meshPickText.clear();
    refreshMeshInspector();
}

void MainWindow::refreshMeshInspector()
{
    if (!m_meshInspectText || !m_meshVisual) return;
    QStringList lines;
    const auto& model = m_meshVisual->model();
    if (!m_meshVisual->visible() || !m_meshVisual->mesh())
        lines << tr("Aucun maillage affiché.");
    else
    {
        const FeMesh& mesh = *m_meshVisual->mesh();
        const auto byDim = model.countByDimension();
        lines << (m_meshVisual->mode() == TSA::Viewer::MeshVisual::Mode::Preview ? tr("Aperçu du maillage (non enregistré)") : tr("Maillage du projet"));
        lines << tr("Maillage : %1 nœuds, %2 éléments, %3 groupes").arg(mesh.nodes.size()).arg(mesh.elements.size()).arg(mesh.groups.size());
        lines << tr("Affichés : %1 nœuds, %2 éléments (1D : %3, 2D : %4, 3D : %5)")
                     .arg(model.visibleNodes().size())
                     .arg(model.visibleElements().size())
                     .arg(byDim[1])
                     .arg(byDim[2])
                     .arg(byDim[3]);
        lines << tr("Arêtes uniques tracées : %1").arg(m_meshVisual->edgeSegmentCount());
        if (byDim[3] > 0)
            lines << tr("Faces des volumes : %1 extérieures, %2 intérieures (%3)")
                         .arg(model.exteriorFaceCount())
                         .arg(model.interiorFaceCount())
                         .arg(model.filter().volumeExteriorOnly ? tr("extérieures affichées") : tr("toutes affichées"));
        const auto& f = model.filter();
        if (!f.isolatedGroups.empty())
        {
            QStringList g;
            for (const auto& n : f.isolatedGroups) g << QString::fromStdString(n);
            lines << tr("Groupes isolés : %1").arg(g.join(", "));
        }
        if (f.cut)
            lines << tr("Coupe : éléments dont le centre vérifie (%1 ; %2 ; %3) · c ≤ %4 m")
                         .arg(f.cutNormal[0])
                         .arg(f.cutNormal[1])
                         .arg(f.cutNormal[2])
                         .arg(f.cutOffset);
        if (m_meshVisual->labelCount() > 0)
            lines << tr("Étiquettes : %1%2").arg(m_meshVisual->labelCount()).arg(m_meshVisual->labelsTruncated() ? tr(" (sous-ensemble réparti)") : QString());
        if (m_meshVisual->hasResults())
            lines << tr("Résultats : déplacements, facteur de déformation %1").arg(m_meshVisual->deformationScale(), 0, 'g', 4);
        lines << tr("Construction de l'affichage : %1 ms").arg(m_meshVisual->lastBuildMs(), 0, 'f', 1);
        if (!model.issues().empty())
        {
            lines << tr("Éléments écartés de l'affichage : %1").arg(model.issues().size());
            for (std::size_t i = 0; i < model.issues().size() && i < 20; ++i)
                lines << tr("  • élément %1 : %2").arg(model.issues()[i].elementId).arg(QString::fromStdString(model.issues()[i].message));
        }
    }
    if (!m_meshPickText.isEmpty()) lines << QString() << tr("— Sélection —") << m_meshPickText;
    m_meshInspectText->setPlainText(lines.join('\n'));
}
