// Maillage (docs/MESH.md) : l'interface relie le panneau « Paramètres du maillage », la liste de qualité,
// l'affichage du maillage dans la vue 3D et la transmission au calcul au contrôleur de maillage (Gmsh).

#include "MainWindow.h"

#include "../Analysis/AnalysisController.h"
#include "../Mesh/MeshGenerator.h"
#include "../Model/Model.h"
#include "../Viewer/MeshOverlay.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "Dock/LogConsoleDock.h"
#include "Mesh/MeshController.h"
#include "Mesh/MeshDialogs.h"
#include "Mesh/MeshSettingsPanel.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

using namespace TSA::Mesh;

namespace
{
std::vector<std::string> selectionRefs(const TSA::Model::ElementSet& s)
{
    std::vector<std::string> r;
    for (int id : s.beams) r.push_back("B" + std::to_string(id));
    for (int id : s.columns) r.push_back("C" + std::to_string(id));
    for (int id : s.trussMembers) r.push_back("T" + std::to_string(id));
    for (int id : s.cables) r.push_back("K" + std::to_string(id));
    for (int id : s.slabs) r.push_back("SL" + std::to_string(id));
    for (int id : s.walls) r.push_back("W" + std::to_string(id));
    for (int id : s.foundations) r.push_back("F" + std::to_string(id));
    return r;
}
} // namespace

void MainWindow::setupMesh()
{
    m_meshController = new TSA::UI::MeshController(this);
    m_meshPanel = new TSA::UI::MeshSettingsPanel(m_meshController, this);
    m_meshDock = new QDockWidget(tr("Paramètres du maillage"), this);
    m_meshDock->setObjectName(QStringLiteral("MeshSettingsDock"));
    m_meshDock->setWidget(m_meshPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_meshDock);
    m_meshDock->hide();
    m_meshQualityPanel = new TSA::UI::MeshQualityPanel(this);
    m_meshQualityDock = new QDockWidget(tr("Qualité du maillage"), this);
    m_meshQualityDock->setObjectName(QStringLiteral("MeshQualityDock"));
    m_meshQualityDock->setWidget(m_meshQualityPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_meshQualityDock);
    m_meshQualityDock->hide();
    if (m_occView) m_meshOverlay = std::make_unique<TSA::Viewer::MeshOverlay>(m_occView->context(), m_occView->view());

    connect(m_meshPanel, &TSA::UI::MeshSettingsPanel::qualityRequested, this, &MainWindow::onActionMeshQuality);
    connect(m_meshPanel, &TSA::UI::MeshSettingsPanel::exportRequested, this, &MainWindow::onActionMeshExport);
    connect(m_meshPanel, &TSA::UI::MeshSettingsPanel::importRequested, this, &MainWindow::onActionMeshImport);
    connect(m_meshPanel, &TSA::UI::MeshSettingsPanel::localForSelectionRequested, this, &MainWindow::onActionMeshLocal);
    connect(m_meshPanel, &TSA::UI::MeshSettingsPanel::zoneFromSelectionRequested, this, &MainWindow::onActionMeshZone);
    connect(m_meshQualityPanel, &TSA::UI::MeshQualityPanel::issueActivated, this, [this](int element, int node) {
        const FeMesh* mesh = m_meshController->hasMesh() ? &m_meshController->project().mesh : nullptr;
        if (m_meshOverlay && mesh) m_meshOverlay->highlight(*mesh, element, node, true);
    });
    connect(m_meshController, &TSA::UI::MeshController::generationFinished, this, [this](bool preview, bool ok, const QString& message) {
        if (m_consoleDock)
        {
            m_consoleDock->appendLog((preview ? tr("Aperçu du maillage (Gmsh) : %1") : tr("Maillage généré (Gmsh) : %1")).arg(message), ok ? "INFO" : "ERROR");
            const auto* rec = preview ? m_meshController->previewRecord() : &m_meshController->project().record;
            if (ok && rec)
            {
                for (const auto& w : rec->warnings) m_consoleDock->appendLog(QString::fromStdString(w), "WARN");
                for (const auto& i : rec->interfaces)
                    if (!i.conforming)
                        m_consoleDock->appendLog(tr("Interface non conforme %1 / %2 : %3").arg(QString::fromStdString(i.a), QString::fromStdString(i.b),
                                                                                               QString::fromStdString(i.description)),
                                                 "WARN");
            }
        }
        if (m_statusInfo) m_statusInfo->setText(TSA::UI::meshStatusText(*m_meshController));
        if (ok && !preview)
        {
            onActionMeshQuality();
            if (m_actionMeshShow) m_actionMeshShow->setChecked(true);
        }
        refreshMeshOverlay();
    });
    connect(m_meshController, &TSA::UI::MeshController::previewChanged, this, &MainWindow::refreshMeshOverlay);
    connect(m_meshController, &TSA::UI::MeshController::meshChanged, this, &MainWindow::refreshMeshOverlay);
    m_meshController->setModel(m_model);
}

void MainWindow::refreshMeshOverlay()
{
    if (!m_meshOverlay || !m_meshController) return;
    if (const FeMesh* preview = m_meshController->preview())
    {
        m_meshOverlay->show(*preview, TSA::Viewer::MeshOverlay::Mode::Preview);
        return;
    }
    if (m_actionMeshShow && m_actionMeshShow->isChecked() && m_meshController->hasMesh())
    {
        const auto& q = m_meshController->quality().perElementQuality;
        m_meshOverlay->show(m_meshController->project().mesh, TSA::Viewer::MeshOverlay::Mode::Generated, &q, 0.1);
    }
    else
        m_meshOverlay->clear();
}

void MainWindow::onMeshModelChanged()
{
    // Appelé à chaque révision du modèle : maillage signalé obsolète, aperçu retiré (il ne correspond plus).
    if (!m_meshController) return;
    if (m_meshController->preview()) m_meshController->discardPreview();
    if (m_meshPanel) m_meshPanel->refresh();
}

void MainWindow::onActionMeshGen()
{
    if (!m_model || !m_meshController) return;
    if (!meshEngineCapabilities().available)
    {
        QMessageBox::warning(this, tr("Maillage"), QString::fromStdString(meshEngineCapabilities().unavailableReason));
        return;
    }
    if (m_model->nodes().empty())
    {
        QMessageBox::information(this, tr("Maillage"), tr("Le modèle est vide. Ajoutez des éléments avant de générer le maillage."));
        return;
    }
    m_meshController->generate(false);
}

void MainWindow::onActionMeshSettings()
{
    if (!m_meshDock) return;
    m_meshPanel->refresh();
    m_meshDock->show();
    m_meshDock->raise();
}

void MainWindow::onActionMeshPreview()
{
    if (!m_meshController) return;
    if (m_meshController->preview())
    {
        // Deuxième appel : l'aperçu est retiré (le modèle et le maillage du projet sont inchangés).
        m_meshController->discardPreview();
        return;
    }
    m_meshController->generate(true);
}

void MainWindow::onActionMeshRegenerate()
{
    if (m_meshController) m_meshController->generate(false);
}

void MainWindow::onActionMeshQuality()
{
    if (!m_meshController || !m_meshQualityPanel) return;
    const bool has = m_meshController->hasMesh();
    QString context = tr("Maillage du projet");
    if (has)
    {
        const auto& rec = m_meshController->project().record;
        context = tr("%1 %2 — %3").arg(QString::fromStdString(rec.mesher), QString::fromStdString(rec.mesherVersion), QString::fromStdString(rec.generated));
        if (m_meshController->stale()) context += tr(" — OBSOLÈTE : géométrie modifiée depuis la génération");
    }
    m_meshQualityPanel->setReport(m_meshController->quality(), has ? &m_meshController->project().mesh : nullptr, context);
    m_meshQualityDock->show();
    m_meshQualityDock->raise();
}

void MainWindow::onActionMeshShow(bool on)
{
    Q_UNUSED(on);
    refreshMeshOverlay();
}

void MainWindow::onActionMeshLocal()
{
    if (!m_meshController || !m_selectionManager) return;
    const auto refs = selectionRefs(m_selectionManager->selectedElements());
    if (refs.empty())
    {
        QMessageBox::information(this, tr("Maillage local"), tr("Sélectionnez des barres, dalles, voiles ou semelles dans la vue 3D."));
        return;
    }
    const auto* existing = localSettingFor(m_meshController->settings(), refs.front());
    TSA::UI::LocalMeshDialog dlg(refs, existing, this);
    if (dlg.exec() != QDialog::Accepted) return;
    auto& locals = m_meshController->settings().locals;
    // Les objets sélectionnés quittent leurs anciens paramètres locaux (les autres objets sont inchangés).
    for (auto& l : locals)
        l.targets.erase(std::remove_if(l.targets.begin(), l.targets.end(), [&](const std::string& t) { return std::find(refs.begin(), refs.end(), t) != refs.end(); }),
                        l.targets.end());
    locals.erase(std::remove_if(locals.begin(), locals.end(), [](const LocalMeshSetting& l) { return l.targets.empty(); }), locals.end());
    locals.push_back(dlg.result());
    m_meshController->commitSettings();
    if (m_meshPanel) m_meshPanel->refresh();
}

void MainWindow::onActionMeshZone()
{
    if (!m_meshController || !m_selectionManager) return;
    const auto sel = m_selectionManager->selectedElements();
    if (sel.nodes.empty())
    {
        QMessageBox::information(this, tr("Raffinement local"),
                                 tr("Sélectionnez un ou plusieurs nœuds (appui, point d'application d'une charge…) dans la vue 3D.\n"
                                    "Les zones en un point, autour de tous les appuis ou de toutes les charges sont dans le panneau Paramètres du maillage."));
        return;
    }
    bool ok = false;
    const double size = QInputDialog::getDouble(this, tr("Raffinement local"), tr("Taille des éléments autour des nœuds sélectionnés (m) :"), 0.1, 0.001, 100, 3, &ok);
    if (!ok) return;
    const double radius = QInputDialog::getDouble(this, tr("Raffinement local"), tr("Rayon de la zone (m) :"), 0.5, 0, 100, 2, &ok);
    if (!ok) return;
    for (int id : sel.nodes)
    {
        RefinementZone z;
        z.kind = RefinementZone::Kind::Node;
        z.nodeId = id;
        z.name = tr("autour de N%1").arg(id).toStdString();
        z.size = size;
        z.radius = radius;
        m_meshController->settings().zones.push_back(z);
    }
    m_meshController->commitSettings();
    if (m_meshPanel) m_meshPanel->refresh();
    if (m_statusInfo) m_statusInfo->setText(tr("%1 zone(s) de raffinement ajoutée(s) : régénérer le maillage pour les appliquer").arg(sel.nodes.size()));
}

void MainWindow::onActionMeshExport()
{
    if (!m_meshController || !m_meshController->hasMesh())
    {
        QMessageBox::information(this, tr("Exporter le maillage"), tr("Aucun maillage généré."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Exporter le maillage"), tr("maillage.msh"),
                                                      tr("Gmsh (*.msh);;VTK (*.vtk);;I-DEAS universel (*.unv);;Abaqus (*.inp);;Nastran (*.bdf);;Medit (*.mesh)"));
    if (path.isEmpty()) return;
    QString err;
    if (!m_meshController->exportMesh(path, &err)) QMessageBox::critical(this, tr("Exporter le maillage"), err);
    else if (m_statusInfo) m_statusInfo->setText(tr("Maillage exporté : %1").arg(path));
}

void MainWindow::onActionMeshImport()
{
    if (!m_meshController) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Importer un maillage"), QString(),
                                                      tr("Maillages (*.msh *.unv *.inp *.bdf *.mesh *.vtk);;Tous les fichiers (*.*)"));
    if (path.isEmpty()) return;
    QStringList warnings;
    QString err;
    if (!m_meshController->importMesh(path, &warnings, &err))
    {
        QMessageBox::critical(this, tr("Importer un maillage"), err);
        return;
    }
    if (m_actionMeshShow) m_actionMeshShow->setChecked(true);
    refreshMeshOverlay();
    QMessageBox::information(this, tr("Importer un maillage"),
                             tr("Maillage importé.\n\n%1\n\nUn maillage importé n'est transmis au calcul que s'il correspond aux objets du modèle.").arg(warnings.join('\n')));
}

void MainWindow::onActionMeshClear()
{
    if (m_meshController && m_meshController->hasMesh() &&
        QMessageBox::question(this, tr("Supprimer le maillage"), tr("Supprimer le maillage du projet ? Les paramètres sont conservés.")) == QMessageBox::Yes)
        m_meshController->clearMesh();
}

void MainWindow::onActionSlabOpenings()
{
    if (!m_model || !m_selectionManager) return;
    const auto sel = m_selectionManager->selectedElements();
    if (sel.slabs.size() != 1)
    {
        QMessageBox::information(this, tr("Ouvertures de dalle"), tr("Sélectionnez une dalle."));
        return;
    }
    const int id = *sel.slabs.begin();
    TSA::UI::SlabOpeningsDialog dlg(*m_model, id, this);
    if (dlg.exec() != QDialog::Accepted) return;
    if (auto* slab = m_model->getSlab(id))
    {
        slab->setOpenings(dlg.openings());
        m_model->setModified(true);
        if (m_occView) m_occView->updateSlabShape(id, true);
        onMeshModelChanged();
        if (m_statusInfo) m_statusInfo->setText(tr("%1 ouverture(s) sur la dalle : régénérer le maillage").arg(dlg.openings().size()));
    }
}

bool MainWindow::applyMeshToAnalysis(TSA::Analysis::AnalysisContext& ctx, const TSA::Analysis::AnalysisEngine& engine)
{
    // Paramètres du mailleur et options du solveur distincts : le maillage n'est transmis que si l'option du
    // calcul le demande, s'il est à jour et compatible avec le moteur (adaptateur OpenSees), sans conversion silencieuse.
    ctx.memberSubdivisions.clear();
    if (!m_meshController || !m_meshController->hasMesh() || !ctx.common.useGeneratedMesh) return true;
    const QString engineName = QString::fromStdString(engine.info().name);
    if (m_meshController->stale())
    {
        QMessageBox box(QMessageBox::Warning, tr("Maillage obsolète"),
                        tr("La géométrie a été modifiée depuis la génération du maillage : il ne correspond plus au modèle."), QMessageBox::NoButton, this);
        auto* regen = box.addButton(tr("Régénérer le maillage"), QMessageBox::AcceptRole);
        auto* without = box.addButton(tr("Calculer sans maillage (un élément par barre)"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == regen) m_meshController->generate(false);
        if (box.clickedButton() != without) return false;
        return true;
    }
    const auto compat = m_meshController->solverCompatibility(engine.info().name, engine.capabilities());
    QStringList lines;
    for (const auto& i : compat.items)
    {
        if (i.level == SolverCheckItem::Level::Ok) continue;
        QString objs;
        for (std::size_t k = 0; k < i.objects.size() && k < 8; ++k) objs += (k ? ", " : " (") + QString::fromStdString(i.objects[k]);
        if (!objs.isEmpty()) objs += i.objects.size() > 8 ? QStringLiteral(", …)") : QStringLiteral(")");
        lines << QStringLiteral("• [%1] %2%3")
                     .arg(i.level == SolverCheckItem::Level::Blocking ? tr("bloquant") : tr("avertissement"), QString::fromStdString(i.message), objs);
    }
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Maillage → %1 : %2").arg(engineName, QString::fromStdString(compat.summary())), "INFO");
        for (const auto& l : lines) m_consoleDock->appendLog(l, "WARN");
    }
    if (compat.blocking())
    {
        QStringList strategies;
        for (const auto& s : compat.strategies) strategies << QStringLiteral("• ") + QString::fromStdString(s);
        QMessageBox box(QMessageBox::Warning, tr("Maillage non transmissible à %1").arg(engineName),
                        tr("%1\n\nStratégies compatibles :\n%2").arg(lines.join('\n'), strategies.join('\n')), QMessageBox::NoButton, this);
        auto* without = box.addButton(tr("Calculer sans maillage (un élément par barre)"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        return box.clickedButton() == without;
    }
    if (!lines.isEmpty())
    {
        if (QMessageBox::warning(this, tr("Maillage et %1").arg(engineName),
                                 tr("Le maillage est transmis en partie :\n\n%1\n\nLancer le calcul ?").arg(lines.join('\n')), QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::Yes) != QMessageBox::Yes)
            return false;
    }
    ctx.memberSubdivisions = compat.memberSubdivisions;
    ctx.meshLabel = tr("maillage %1 du %2").arg(QString::fromStdString(m_meshController->project().record.mesher),
                                                QString::fromStdString(m_meshController->project().record.generated)).toStdString();
    return true;
}
