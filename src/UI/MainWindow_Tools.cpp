#include "Dock/AnalysisDataDock.h"
#include "../Analysis/AnalysisController.h"
#include "MainWindow.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "../Model/Model.h"
#include "Ruler/ViewportContainer.h"
#include "Dock/LogConsoleDock.h"
#include "Dialogs/NodalLoadDialog.h"
#include "Dialogs/NewNodeDialog.h"
#include "Dialogs/MemberLoadDialog.h"
#include "Dialogs/LoadCaseDialog.h"
#include "Diagrams/Diagram2DWidget.h"
#include "../NDC/NDCViewerWidget.h"
#include "../Analysis/OpenSeesSolver.h"
#include "../Analysis/OpenSeesManager.h"
#include "../Analysis/ResultsModel.h"
#include "../Viewer/ResultsVisualManager.h"
#include "Analysis/AnalysisDialog.h"
#include "Analysis/AnalysisEngineOptions.h"
#include "../Analysis/Engine/AnalysisManager.h"
#include "Dock/ResultsDockWidget.h"
#include "Properties/PropertyPanel.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QLabel>
#include <QSignalBlocker>
#include <QProgressDialog>
#include <QEventLoop>
#include <QThread>
#include <QJsonDocument>
#include <QPointer>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <set>
#include <vector>

// =========================================================================
// Outils Métier & Ingénierie des Structures
// =========================================================================

namespace
{
/// Dalles et voiles ne sont pas maillés : ils ne sont pas transmis à OpenSees (BUG-002).
/// L'utilisateur doit le savoir avant de lancer un calcul. Retourne false s'il annule.
bool confirmPlanarElementsExcluded(QWidget* parent, const TSA::Model::Model& model)
{
    const size_t slabs = model.slabs().size();
    const size_t walls = model.walls().size();
    if (slabs == 0 && walls == 0)
        return true;
    const auto reply = QMessageBox::warning(parent,
        QObject::tr("Dalles et voiles non calculés"),
        QObject::tr("Le modèle contient %1 dalle(s) et %2 voile(s).\n\n"
                    "TSA ne maille pas encore ces éléments : ils ne sont PAS transmis à OpenSees "
                    "(ni leur rigidité, ni leurs charges). Seuls les éléments filaires (poutres, "
                    "poteaux, treillis, câbles) sont calculés.\n\n"
                    "Lancer quand même le calcul ?").arg(slabs).arg(walls),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return reply == QMessageBox::Yes;
}
} // namespace

void MainWindow::onActionWall()
{
    onModeDrawWall();
}

void MainWindow::onActionTruss()
{
    if (!m_model) return;

    QStringList types = { tr("Warren (Diagonales alternées)"), tr("Pratt (Diagonales tendues)"), tr("Howe (Diagonales comprimées)") };
    bool ok = false;
    QString chosenType = QInputDialog::getItem(this, tr("Générateur de Treillis"), tr("Type de treillis métallique :"), types, 0, false, &ok);
    if (!ok) return;

    double span = QInputDialog::getDouble(this, tr("Portée du Treillis"), tr("Portée totale L (m) :"), 12.0, 2.0, 100.0, 2, &ok);
    if (!ok) return;

    double height = QInputDialog::getDouble(this, tr("Hauteur du Treillis"), tr("Hauteur H (m) :"), 1.80, 0.3, 20.0, 2, &ok);
    if (!ok) return;

    int panels = QInputDialog::getInt(this, tr("Nombre de Panneaux"), tr("Nombre de mailles N :"), 6, 2, 40, 2, &ok);
    if (!ok) return;

    double x0 = 0.0, y0 = 0.0, z0 = 0.0;
    if (m_selectionManager && !m_selectionManager->selectedNodes().empty())
    {
        int originNodeId = *m_selectionManager->selectedNodes().begin();
        const auto* orig = m_model->getNode(originNodeId);
        if (orig) { x0 = orig->x(); y0 = orig->y(); z0 = orig->z(); }
    }
    else if (m_viewportContainer)
    {
        z0 = m_viewportContainer->activeLevelElevation();
    }

    double dx = span / panels;
    std::vector<int> botNodes(panels + 1);
    std::vector<int> topNodes(panels + 1);

    for (int i = 0; i <= panels; ++i)
    {
        botNodes[i] = m_model->addNode(x0 + i * dx, y0, z0);
        topNodes[i] = m_model->addNode(x0 + i * dx, y0, z0 + height);
    }

    int beamCount = 0;
    // Membrure inférieure et supérieure
    for (int i = 0; i < panels; ++i)
    {
        m_model->addBeam(botNodes[i], botNodes[i + 1], 0.20, 0.20);
        m_model->addBeam(topNodes[i], topNodes[i + 1], 0.20, 0.20);
        beamCount += 2;
    }

    // Montants verticaux
    for (int i = 0; i <= panels; ++i)
    {
        m_model->addBeam(botNodes[i], topNodes[i], 0.15, 0.15);
        beamCount++;
    }

    // Diagonales selon le type choisi
    int mid = panels / 2;
    for (int i = 0; i < panels; ++i)
    {
        if (chosenType.startsWith("Warren"))
        {
            if (i % 2 == 0) m_model->addBeam(botNodes[i], topNodes[i + 1], 0.15, 0.15);
            else m_model->addBeam(topNodes[i], botNodes[i + 1], 0.15, 0.15);
            beamCount++;
        }
        else if (chosenType.startsWith("Pratt"))
        {
            if (i < mid) m_model->addBeam(topNodes[i], botNodes[i + 1], 0.15, 0.15);
            else m_model->addBeam(botNodes[i], topNodes[i + 1], 0.15, 0.15);
            beamCount++;
        }
        else // Howe
        {
            if (i < mid) m_model->addBeam(botNodes[i], topNodes[i + 1], 0.15, 0.15);
            else m_model->addBeam(topNodes[i], botNodes[i + 1], 0.15, 0.15);
            beamCount++;
        }
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Treillis %1 généré : %2 nœuds, %3 barres (L = %4 m, H = %5 m, %6 panneaux)")
            .arg(chosenType).arg(botNodes.size() + topNodes.size()).arg(beamCount).arg(span).arg(height).arg(panels), "SUCCESS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Treillis créé (%1 barres)").arg(beamCount));
    }
}

void MainWindow::onActionFooting()
{
    if (!m_model) return;

    std::set<int> baseNodes;
    if (m_selectionManager && !m_selectionManager->selectedColumns().empty())
    {
        for (int cId : m_selectionManager->selectedColumns())
        {
            const auto* c = m_model->getColumn(cId);
            if (c) baseNodes.insert(c->startNodeId());
        }
    }
    else if (m_selectionManager && !m_selectionManager->selectedNodes().empty())
    {
        baseNodes = m_selectionManager->selectedNodes();
    }
    else
    {
        double minZ = 1e9;
        for (const auto& [id, n] : m_model->nodes())
        {
            if (n.z() < minZ) minZ = n.z();
        }
        for (const auto& [id, n] : m_model->nodes())
        {
            if (std::abs(n.z() - minZ) < 1e-3)
            {
                baseNodes.insert(id);
            }
        }
    }

    if (baseNodes.empty())
    {
        QMessageBox::information(this, tr("Semelles"), tr("Aucun nœud d'appui ou pied de poteau trouvé."));
        return;
    }

    bool ok = false;
    double a = QInputDialog::getDouble(this, tr("Semelle Isolée"), tr("Largeur A (m) :"), 1.50, 0.4, 10.0, 2, &ok);
    if (!ok) return;
    double b = QInputDialog::getDouble(this, tr("Semelle Isolée"), tr("Longueur B (m) :"), 1.50, 0.4, 10.0, 2, &ok);
    if (!ok) return;
    double h = QInputDialog::getDouble(this, tr("Semelle Isolée"), tr("Épaisseur H (m) :"), 0.45, 0.2, 5.0, 2, &ok);
    if (!ok) return;

    int footingCount = 0;
    for (int nid : baseNodes)
    {
        const auto* n = m_model->getNode(nid);
        if (!n) continue;
        double x = n->x(), y = n->y(), z = n->z();
        int fn1 = m_model->addNode(x - a / 2.0, y - b / 2.0, z - h);
        int fn2 = m_model->addNode(x + a / 2.0, y - b / 2.0, z - h);
        int fn3 = m_model->addNode(x + a / 2.0, y + b / 2.0, z - h);
        int fn4 = m_model->addNode(x - a / 2.0, y + b / 2.0, z - h);
        m_model->addSlab({ fn1, fn2, fn3, fn4 }, h);
        footingCount++;
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Génération de %1 semelle(s) isolée(s) BA (%2m x %3m, h=%4m) avec liaison au sol.")
            .arg(footingCount).arg(a).arg(b).arg(h), "SUCCESS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("%1 semelle(s) isolée(s) BA générée(s)").arg(footingCount));
    }
}

void MainWindow::onActionSecI()
{
    QStringList catalog = {
        "IPE 160 (160 x 82 mm, Iy=869 cm4, Iz=68.3 cm4, A=20.1 cm2)",
        "IPE 200 (200 x 100 mm, Iy=1943 cm4, Iz=142 cm4, A=28.5 cm2)",
        "IPE 240 (240 x 120 mm, Iy=3892 cm4, Iz=284 cm4, A=39.1 cm2)",
        "IPE 270 (270 x 135 mm, Iy=5790 cm4, Iz=420 cm4, A=45.9 cm2)",
        "IPE 300 (300 x 150 mm, Iy=8356 cm4, Iz=604 cm4, A=53.8 cm2)",
        "IPE 360 (360 x 170 mm, Iy=16270 cm4, Iz=1043 cm4, A=72.7 cm2)",
        "IPE 400 (400 x 180 mm, Iy=23130 cm4, Iz=1318 cm4, A=84.5 cm2)",
        "HEA 200 (190 x 200 mm, Iy=3690 cm4, Iz=1340 cm4, A=53.8 cm2)",
        "HEA 240 (230 x 240 mm, Iy=7760 cm4, Iz=2770 cm4, A=76.8 cm2)",
        "HEB 200 (200 x 200 mm, Iy=5700 cm4, Iz=2000 cm4, A=78.1 cm2)",
        "HEB 300 (300 x 300 mm, Iy=25170 cm4, Iz=8560 cm4, A=149.0 cm2)"
    };

    bool ok = false;
    QString choice = QInputDialog::getItem(this, tr("Catalogue Profilés Métalliques"), tr("Sélectionnez le profilé en I/H :"), catalog, 4, false, &ok);
    if (!ok) return;

    TSA::Model::Section sec;
    if (choice.startsWith("IPE 160")) sec = TSA::Model::Section::ipe(160);
    else if (choice.startsWith("IPE 200")) sec = TSA::Model::Section::ipe(200);
    else if (choice.startsWith("IPE 240")) sec = TSA::Model::Section::ipe(240);
    else if (choice.startsWith("IPE 270")) sec = TSA::Model::Section::ipe(270);
    else if (choice.startsWith("IPE 300")) sec = TSA::Model::Section::ipe(300);
    else if (choice.startsWith("IPE 360")) sec = TSA::Model::Section::ipe(360);
    else if (choice.startsWith("IPE 400")) sec = TSA::Model::Section::ipe(400);
    else if (choice.startsWith("HEA 200")) sec = TSA::Model::Section::hea(200);
    else if (choice.startsWith("HEA 240")) sec = TSA::Model::Section::hea(240);
    else if (choice.startsWith("HEB 200")) sec = TSA::Model::Section::heb(200);
    else if (choice.startsWith("HEB 300")) sec = TSA::Model::Section::heb(300);
    else sec = TSA::Model::Section::ipe(200);

    double h = sec.height;
    double b = sec.width;

    int modified = 0;
    if (m_selectionManager)
    {
        for (int bId : m_selectionManager->selectedBeams())
        {
            auto* bm = m_model->getBeam(bId);
            if (bm)
            {
                bm->setSection(sec);
                m_model->notifyBeamModified(bId);
                modified++;
            }
        }
        for (int cId : m_selectionManager->selectedColumns())
        {
            auto* col = m_model->getColumn(cId);
            if (col)
            {
                col->setSection(sec);
                m_model->notifyColumnModified(cId);
                modified++;
            }
        }
    }

    m_presets.beam.section = sec;
    m_presets.column.section = sec;
    if (m_occView) m_occView->setCreationPresets(m_presets);

    QString profName = choice.split(" ").value(0) + " " + choice.split(" ").value(1);
    if (m_consoleDock)
    {
        if (modified > 0)
            m_consoleDock->appendLog(tr("Profilé %1 appliqué à %2 barre(s) (h=%3m, b=%4m)").arg(profName).arg(modified).arg(h).arg(b), "SUCCESS");
        else
            m_consoleDock->appendLog(tr("Profilé par défaut : %1 (h=%2m, b=%3m). Sélectionnez des barres pour l'assigner.").arg(profName).arg(h).arg(b), "INFO");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Profilé %1 sélectionné").arg(profName));
    }
}

void MainWindow::onActionSecRect()
{
    bool ok = false;
    double b = QInputDialog::getDouble(this, tr("Section Rectangulaire BA"), tr("Largeur b (m) :"), 0.30, 0.05, 5.0, 2, &ok);
    if (!ok) return;
    double h = QInputDialog::getDouble(this, tr("Section Rectangulaire BA"), tr("Hauteur h (m) :"), 0.50, 0.05, 5.0, 2, &ok);
    if (!ok) return;

    std::string secName = QString("R%1x%2").arg(b * 100, 0, 'f', 0).arg(h * 100, 0, 'f', 0).toStdString();
    auto sec = TSA::Model::Section::rectangular(b, h, secName);

    int modified = 0;
    if (m_selectionManager)
    {
        for (int bId : m_selectionManager->selectedBeams())
        {
            auto* bm = m_model->getBeam(bId);
            if (bm)
            {
                bm->setSection(sec);
                m_model->notifyBeamModified(bId);
                modified++;
            }
        }
        for (int cId : m_selectionManager->selectedColumns())
        {
            auto* col = m_model->getColumn(cId);
            if (col)
            {
                col->setSection(sec);
                m_model->notifyColumnModified(cId);
                modified++;
            }
        }
    }

    m_presets.beam.section = sec;
    m_presets.column.section = sec;
    if (m_occView) m_occView->setCreationPresets(m_presets);

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Section Rectangulaire (%1 x %2 m) appliquée à %3 élément(s)").arg(b).arg(h).arg(modified), "SUCCESS");
    }
}

void MainWindow::onActionSecCirc()
{
    bool ok = false;
    double d = QInputDialog::getDouble(this, tr("Section Circulaire"), tr("Diamètre D (m) :"), 0.60, 0.05, 5.0, 2, &ok);
    if (!ok) return;

    std::string secName = QString("D%1").arg(d * 100, 0, 'f', 0).toStdString();
    auto sec = TSA::Model::Section::circular(d, secName);

    int modified = 0;
    if (m_selectionManager)
    {
        for (int bId : m_selectionManager->selectedBeams())
        {
            auto* bm = m_model->getBeam(bId);
            if (bm)
            {
                bm->setSection(sec);
                m_model->notifyBeamModified(bId);
                modified++;
            }
        }
        for (int cId : m_selectionManager->selectedColumns())
        {
            auto* col = m_model->getColumn(cId);
            if (col)
            {
                col->setSection(sec);
                m_model->notifyColumnModified(cId);
                modified++;
            }
        }
    }

    m_presets.beam.section = sec;
    m_presets.column.section = sec;
    if (m_occView) m_occView->setCreationPresets(m_presets);

    if (m_consoleDock)
    {
        if (modified > 0)
        {
            m_consoleDock->appendLog(tr("Section Circulaire %1 (Ø%2 m) appliquée à %3 élément(s)")
                .arg(QString::fromStdString(secName)).arg(d).arg(modified), "SUCCESS");
        }
        else
        {
            m_consoleDock->appendLog(tr("Section Circulaire %1 (Ø%2 m) définie comme section par défaut")
                .arg(QString::fromStdString(secName)).arg(d), "INFO");
        }
    }
}

void MainWindow::onActionConcrete()
{
    QStringList concretes = {
        "Béton C20/25 (fck = 20 MPa, Ecm = 30 GPa, rho = 25 kN/m³)",
        "Béton C25/30 (fck = 25 MPa, Ecm = 31 GPa, rho = 25 kN/m³) - Standard EC2",
        "Béton C30/37 (fck = 30 MPa, Ecm = 33 GPa, rho = 25 kN/m³)",
        "Béton C35/45 (fck = 35 MPa, Ecm = 34 GPa, rho = 25 kN/m³)"
    };
    bool ok = false;
    QString choice = QInputDialog::getItem(this, tr("Matériaux - Béton Armé"), tr("Nuance de béton Eurocode 2 :"), concretes, 1, false, &ok);
    if (!ok) return;

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Matériau assigné : %1").arg(choice), "SUCCESS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Matériau : %1").arg(choice.split(" ").value(0) + " " + choice.split(" ").value(1)));
    }
}

void MainWindow::onActionSteel()
{
    QStringList steels = {
        "Acier S235 (fy = 235 MPa, fu = 360 MPa, E = 210 GPa, rho = 78.5 kN/m³)",
        "Acier S275 (fy = 275 MPa, fu = 430 MPa, E = 210 GPa, rho = 78.5 kN/m³)",
        "Acier S355 (fy = 355 MPa, fu = 510 MPa, E = 210 GPa, rho = 78.5 kN/m³) - Standard EC3",
        "Acier S460 (fy = 460 MPa, fu = 540 MPa, E = 210 GPa, rho = 78.5 kN/m³)"
    };
    bool ok = false;
    QString choice = QInputDialog::getItem(this, tr("Matériaux - Acier Structural"), tr("Nuance d'acier Eurocode 3 :"), steels, 2, false, &ok);
    if (!ok) return;

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Matériau assigné : %1").arg(choice), "SUCCESS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Matériau : %1").arg(choice.split(" ").value(0) + " " + choice.split(" ").value(1)));
    }
}

void MainWindow::onActionFixed()
{
    std::set<int> targetNodes;
    if (m_selectionManager && !m_selectionManager->selectedNodes().empty())
    {
        targetNodes = m_selectionManager->selectedNodes();
    }
    else
    {
        double minZ = 1e9;
        for (const auto& [id, n] : m_model->nodes())
        {
            if (n.z() < minZ) minZ = n.z();
        }
        for (const auto& [id, n] : m_model->nodes())
        {
            if (std::abs(n.z() - minZ) < 1e-3) targetNodes.insert(id);
        }
    }

    if (targetNodes.empty())
    {
        QMessageBox::information(this, tr("Appui Encastré"), tr("Aucun nœud d'appui sélectionné."));
        return;
    }

    assignSupport(targetNodes, TSA::Model::SupportDefinition::fixed(), tr("Encastrement (6 DDL bloqués)"));
}

void MainWindow::onActionPinned()
{
    std::set<int> targetNodes = m_selectionManager ? m_selectionManager->selectedNodes() : std::set<int>{};
    if (targetNodes.empty())
    {
        QMessageBox::information(this, tr("Appui Articulé"), tr("Veuillez sélectionner au moins un nœud."));
        return;
    }
    assignSupport(targetNodes, TSA::Model::SupportDefinition::pinned(), tr("Articulation (Tx, Ty, Tz bloqués)"));
}

void MainWindow::onActionRoller()
{
    std::set<int> targetNodes = m_selectionManager ? m_selectionManager->selectedNodes() : std::set<int>{};
    if (targetNodes.empty())
    {
        QMessageBox::information(this, tr("Appui Simple"), tr("Veuillez sélectionner au moins un nœud."));
        return;
    }
    assignSupport(targetNodes, TSA::Model::SupportDefinition::roller(), tr("Appui simple (Tz bloqué)"));
}

void MainWindow::assignSupport(const std::set<int>& nodeIds, const TSA::Model::SupportDefinition& support, const QString& label)
{
    if (!m_model || nodeIds.empty()) return;

    // Une seule entrée Annuler pour tout le lot, puis notification par nœud (vues, invalidation des résultats).
    m_model->pushUndoState(tr("Appui : %1").arg(label).toStdString());
    QStringList ids;
    for (int id : nodeIds)
    {
        if (auto* node = m_model->getNode(id))
        {
            node->setSupport(support);
            m_model->notifyNodeModified(id);
            ids << QString("#%1").arg(id);
        }
    }
    updateUndoRedoActions();
    updateWindowTitle();

    if (m_consoleDock)
        m_consoleDock->appendLog(tr("%1 assigné à %2 nœud(s) : %3").arg(label).arg(ids.size()).arg(ids.join(", ")), "SUCCESS");
    if (m_statusInfo)
        m_statusInfo->setText(tr("%1 assigné (%2 nœud(s))").arg(label).arg(ids.size()));
}

void MainWindow::onActionPointLoad()
{
    if (!m_model) return;
    // Non modale : le bouton « Sélectionner 3D » de la fenêtre doit pouvoir cliquer dans la vue
    // (en modal, la vue était bloquée ; fermer puis cliquer provoquait un crash).
    if (!m_nodalLoadDialog)
    {
        m_nodalLoadDialog = new TSA::UI::NodalLoadDialog(m_model, m_selectionManager.get(), m_occView, this);
        m_nodalLoadDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_nodalLoadDialog->setModal(false);
    }
    if (m_selectionManager && !m_selectionManager->selectedNodes().empty())
    {
        m_nodalLoadDialog->setTargetNodeId(*m_selectionManager->selectedNodes().begin());
    }
    m_nodalLoadDialog->show();
    m_nodalLoadDialog->raise();
    m_nodalLoadDialog->activateWindow();
}

void MainWindow::closeModelessToolDialogs()
{
    // Projet remplacé ou fermé : leurs listes (nœuds, cas de charge) appartiennent à l'ancien projet.
    if (m_nodalLoadDialog) m_nodalLoadDialog->close();
    if (m_newNodeDialog) m_newNodeDialog->close();
}

void MainWindow::onActionDistLoad()
{
    if (!m_model) return;
    TSA::UI::MemberLoadDialog dlg(m_model, m_selectionManager.get(), m_occView, this);
    if (m_selectionManager)
    {
        if (!m_selectionManager->selectedBeams().empty())
        {
            dlg.setTargetElementId(*m_selectionManager->selectedBeams().begin());
        }
        else if (!m_selectionManager->selectedColumns().empty())
        {
            dlg.setTargetElementId(*m_selectionManager->selectedColumns().begin(), TSA::Model::MemberTargetType::Column);
        }
    }
    dlg.exec();
}

void MainWindow::onActionLoadCases()
{
    if (!m_model) return;
    TSA::UI::LoadCaseDialog dlg(m_model, this);
    dlg.exec();
}

void MainWindow::onActionMeshGen()
{
    if (!m_model) return;

    if (m_model->nodes().empty())
    {
        QMessageBox::information(this, tr("Maillage"), tr("Le modèle est vide. Ajoutez des éléments avant de générer le maillage."));
        return;
    }

    bool ok = false;
    double hMesh = QInputDialog::getDouble(this, tr("Générateur de Maillage EF"), tr("Taille cible des mailles h (m) :"), 0.50, 0.05, 5.0, 2, &ok);
    if (!ok) return;

    size_t beamElems = m_model->beams().size() * 4;
    size_t colElems = m_model->columns().size() * 4;
    size_t slabElems = m_model->slabs().size() * 16;
    size_t totalElems = beamElems + colElems + slabElems;
    size_t meshNodes = m_model->nodes().size() + totalElems * 2;
    size_t dofs = meshNodes * 6;

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("--- ESTIMATION DU MAILLAGE ÉLÉMENTS FINIS (h = %1 m) ---").arg(hMesh), "SYS");
        m_consoleDock->appendLog(tr("  - Éléments 1D (Poutres & Poteaux Hermite) : %1").arg(beamElems + colElems), "INFO");
        m_consoleDock->appendLog(tr("  - Éléments 2D (Dalles / Coques DKT)       : %1").arg(slabElems), "INFO");
        m_consoleDock->appendLog(tr("  - Nœuds du maillage discrétisé             : %1").arg(meshNodes), "INFO");
        m_consoleDock->appendLog(tr("  - Degrés de liberté (DDL) estimés          : %1").arg(dofs), "INFO");
        m_consoleDock->appendLog(tr("  ! Aucun maillage n'est réellement généré : dalles et voiles ne sont pas transmis au calcul."), "WARN");
    }

    QMessageBox::information(this, tr("Maillage Éléments Finis"),
        tr("Estimation indicative (aucun maillage n'est généré) :\n\n"
           "• Éléments finis estimés : %1\n"
           "• Nœuds de discrétisation : %2\n"
           "• Degrés de liberté (DDL) : %3\n"
           "• Discrétisation spatiale : h = %4 m\n\n"
           "Le mailleur de dalles / voiles n'est pas encore disponible : ces éléments "
           "ne participent pas au calcul OpenSees.")
        .arg(totalElems).arg(meshNodes).arg(dofs).arg(hMesh));

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Maillage EF estimé : %1 éléments, %2 DDL (non généré)").arg(totalElems).arg(dofs));
    }
}

void MainWindow::onActionAnalysisConfig()
{
    if (!m_model || !m_analysis) return;
    const TSA::Model::ElementSet selection = m_selectionManager ? m_selectionManager->selectedElements()
                                                                : TSA::Model::ElementSet{};
    TSA::UI::AnalysisDialog dlg(m_analysis->manager(), *m_engineOptions, m_model, m_gridManager, selection, this);
    dlg.setContext(m_analysis->context());
    if (dlg.exec() != QDialog::Accepted) return;

    m_analysis->setContext(dlg.context());
    storeAnalysisContextInModel();
    if (m_consoleDock)
    {
        const auto* engine = m_analysis->registry().engine(m_analysis->context().engineId);
        m_consoleDock->appendLog(tr("Analyse configurée : moteur %1")
                                     .arg(engine ? QString::fromStdString(engine->info().name) : tr("inconnu")), "INFO");
    }
    if (dlg.runRequested()) runAnalysis(m_analysis->context());
}

void MainWindow::storeAnalysisContextInModel()
{
    if (m_analysis && m_analysis->storeContextInModel()) updateWindowTitle();
}

void MainWindow::restoreAnalysisContextFromModel()
{
    if (m_analysis && !m_analysis->restoreContextFromModel() && m_consoleDock)
        m_consoleDock->appendLog(tr("Paramètres d'analyse du projet illisibles : réglages par défaut."), "WARN");
}

void MainWindow::onActionRunSolve()
{
    runAnalysis(m_analysis->context());
}

bool MainWindow::runAnalysis(const TSA::Analysis::AnalysisContext& context)
{
    using namespace TSA::Analysis;
    if (!m_model || !m_analysis) return false;
    if (m_analysis->isRunning()) return false;

    AnalysisEngine* engine = m_analysis->registry().engine(context.engineId);
    if (!engine)
    {
        QMessageBox::warning(this, tr("Analyse"), tr("Aucun moteur d'analyse sélectionné."));
        return false;
    }
    const QString engineName = QString::fromStdString(engine->info().name);

    // 1. Disponibilité (installation proposée si le moteur sait se provisionner).
    const EngineAvailability avail = engine->availability();
    if (!avail.available)
    {
        if (!avail.canProvision)
        {
            QMessageBox::warning(this, tr("Moteur %1 indisponible").arg(engineName), QString::fromStdString(avail.message));
            return false;
        }
        const auto reply = QMessageBox::question(this, tr("Moteur %1 indisponible").arg(engineName),
            tr("%1\n\nVoulez-vous l'installer automatiquement ?").arg(QString::fromStdString(avail.message)),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return false;
        std::string err;
        if (!engine->provision(&err))
        {
            QMessageBox::critical(this, tr("Installation impossible"), QString::fromStdString(err));
            return false;
        }
    }

    // 1 bis. Topologie : nœuds confondus, nœuds sur barres, doublons, nœuds parasites faussent le
    //        calcul ou la validation → nettoyage proposé (bilan calculé sans modifier le modèle).
    {
        const TSA::Model::CleanupReport check = TSA::Model::ModelCleanup::analyze(*m_model);
        if (check.changed())
        {
            QMessageBox box(QMessageBox::Question, tr("Nettoyage du modèle recommandé"),
                            tr("Le modèle présente des défauts de topologie :\n\n%1\n\n"
                               "Les corriger avant le calcul ? (annulable par Ctrl+Z)").arg(QString::fromStdString(check.summary())),
                            QMessageBox::NoButton, this);
            auto* cleanBtn = box.addButton(tr("Nettoyer puis calculer"), QMessageBox::AcceptRole);
            box.addButton(tr("Calculer sans nettoyer"), QMessageBox::DestructiveRole);
            auto* cancelBtn = box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(cleanBtn);
            box.exec();
            if (box.clickedButton() == cancelBtn) return false;
            if (box.clickedButton() == cleanBtn) applyModelCleanup({});
        }
    }

    // 2. Préparation : portée → modèle d'analyse → validation (générique + moteur).
    const PreparedAnalysis prepared = m_analysis->prepare(context);
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("--- CALCUL %1 — %2 ---").arg(engineName.toUpper(),
                                     QString::fromStdString(prepared.model.scopeLabel)), "SYS");
        for (const auto& m : prepared.validation.messages())
        {
            const char* level = m.severity == ValidationSeverity::Error ? "ERROR"
                              : m.severity == ValidationSeverity::Warning ? "WARN" : "INFO";
            m_consoleDock->appendLog(QString::fromStdString(m.text), level);
        }
    }
    if (!prepared.canRun())
    {
        QStringList errors;
        for (const auto& e : prepared.validation.texts(ValidationSeverity::Error)) errors << QString::fromStdString(e);
        QMessageBox::critical(this, tr("Analyse impossible"),
                              tr("Le modèle d'analyse n'est pas valide :\n\n• %1").arg(errors.join("\n• ")));
        return false;
    }
    if (prepared.validation.hasWarnings())
    {
        QStringList warnings;
        for (const auto& w : prepared.validation.texts(ValidationSeverity::Warning)) warnings << QString::fromStdString(w);
        const auto reply = QMessageBox::warning(this, tr("Avertissements avant calcul"),
            tr("%1\n\nLancer quand même le calcul ?").arg("• " + warnings.join("\n• ")),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return false;
    }

    // 3. Calcul dans un thread de travail : l'interface reste réactive et le calcul est annulable.
    //    Le moteur ne lit que le snapshot préparé ; journal et progression reviennent au thread UI.
    QPointer<QProgressDialog> progress = new QProgressDialog(tr("Calcul %1 en cours…").arg(engineName), tr("Annuler"), 0, 100, this);
    progress->setWindowTitle(tr("Calcul"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(400);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setValue(0);

    // Calcul par le contrôleur partagé (thread de travail) ; une boucle locale attend sa fin pour garder
    // le déroulé séquentiel de cette commande (résumé, message) sans bloquer l'interface.
    QEventLoop loop;
    bool success = false;
    bool cancelledByUser = false;
    QString message;
    const auto logConn = connect(m_analysis, &AnalysisController::logMessage, this, [this](const QString& line) {
        if (m_consoleDock) m_consoleDock->appendLog(line, "INFO");
    });
    const auto progressConn = connect(m_analysis, &AnalysisController::progressChanged, this,
        [this, progress](int pct, const QString& status) {
            if (progress)
            {
                progress->setValue(std::clamp(pct, 0, 99));
                progress->setLabelText(status);
            }
            if (m_statusInfo) m_statusInfo->setText(QStringLiteral("%1 % — %2").arg(pct).arg(status));
        });
    const auto finishedConn = connect(m_analysis, &AnalysisController::finished, &loop,
        [&loop, &success, &message](bool ok, const QString& text) {
            success = ok;
            message = text;
            loop.quit();
        });
    connect(progress, &QProgressDialog::canceled, this, [this, progress, &cancelledByUser] {
        cancelledByUser = true;
        m_analysis->cancel();
        if (progress) progress->setLabelText(tr("Annulation du calcul…"));
    });
    if (m_analysis->start(context, prepared)) loop.exec();
    else message = tr("Le calcul n'a pas pu être lancé.");
    disconnect(logConn);
    disconnect(progressConn);
    disconnect(finishedConn);
    if (progress) progress->deleteLater();

    if (!success && cancelledByUser)
    {
        // Interruption demandée : ce n'est pas une erreur du moteur, aucune boîte critique.
        if (m_consoleDock) m_consoleDock->appendLog(tr("Calcul %1 annulé : %2").arg(engineName, message), "WARN");
        if (m_statusInfo) m_statusInfo->setText(tr("Calcul %1 annulé").arg(engineName));
        return false;
    }
    if (!success)
    {
        if (m_consoleDock)
        {
            m_consoleDock->appendLog(tr("Échec du calcul %1 : %2").arg(engineName, message), "ERROR");
            const std::string& journal = m_analysis->lastRun().results.journalLog();
            if (!journal.empty()) m_consoleDock->appendLog(QString::fromStdString(journal), "ERROR");
        }
        QMessageBox::critical(this, tr("Erreur du moteur %1").arg(engineName),
                              tr("Le calcul a échoué :\n%1").arg(message));
        return false;
    }

    // 4. Résultats publiés par le contrôleur (déjà remappés sur les identifiants TSA par l'adaptateur) :
    //    les vues sont mises à jour par onAnalysisResultsChanged().
    if (!m_resultsModel) return false;

    const auto& meta = m_resultsModel->executionMetadata();
    QString summary = tr("Moteur : %1%2\nPortée : %3\n")
                          .arg(engineName)
                          .arg(meta.solverVersion.empty() ? QString() : " " + QString::fromStdString(meta.solverVersion))
                          .arg(QString::fromStdString(prepared.model.scopeLabel));
    const auto ext = m_resultsModel->summary();
    const auto eq = m_resultsModel->equilibrium();
    summary += tr("\n• Déplacement max : %1 mm (nœud N%2)\n• Moment max : %3 kNm (barre #%4)\n"
                  "• Traction max : %5 kN\n• Réaction verticale totale : %6 kN")
                   .arg(ext.maxDisplacement * 1000.0, 0, 'f', 3).arg(ext.maxDisplacementNodeId)
                   .arg(ext.maxBendingMoment, 0, 'f', 2).arg(ext.maxBendingMomentElementId)
                   .arg(ext.maxTension, 0, 'f', 2).arg(eq.reactionFz, 0, 'f', 2);
    if (m_statusInfo)
        m_statusInfo->setText(tr("%1 OK : δ_max = %2 mm, M_max = %3 kNm")
                                  .arg(engineName).arg(ext.maxDisplacement * 1000.0, 0, 'f', 2)
                                  .arg(ext.maxBendingMoment, 0, 'f', 1));
    if (m_consoleDock) m_consoleDock->appendLog(summary, "SUCCESS");
    QMessageBox::information(this, tr("Calcul terminé"), summary);
    return true;
}

void MainWindow::onAnalysisResultsChanged()
{
    m_resultsModel = m_analysis->results();
    emit resultsChanged();
    if (m_occView) m_occView->setResultsModel(m_resultsModel);
    if (m_diagramWidget) m_diagramWidget->setResultsModel(m_resultsModel);
    if (m_ndcWidget) m_ndcWidget->setResultsModel(m_resultsModel);
    if (m_propertyPanel) m_propertyPanel->setResultsModel(m_resultsModel);
    if (m_analysisDataDock) m_analysisDataDock->setResultsModel(m_resultsModel);
    if (m_resultsDock)
    {
        m_resultsDock->setResultsModel(m_resultsModel);
        if (!m_resultsModel) return;
        if (m_occView && m_occView->resultsVisual()) m_resultsDock->syncFromVisualManager(m_occView->resultsVisual());
        m_resultsDock->show();
        m_resultsDock->raise();
    }
}

void MainWindow::onActionNoteDeCalcul()
{
    if (m_ndcWidget)
    {
        m_ndcWidget->setModel(m_model);
        m_ndcWidget->setResultsModel(m_resultsModel);
        m_ndcWidget->refreshDocument(); // une seule génération (setModel/setResultsModel sont différés)
    }
    if (m_ndcDock)
    {
        m_ndcDock->show();
        m_ndcDock->raise();
    }
}

void MainWindow::onActionToggleDeformed(bool checked)
{
    // Garder la case du ruban/menu cohérente quand l'appel vient du dock Résultats.
    if (m_actionDeformedToggle && m_actionDeformedToggle->isChecked() != checked)
    {
        QSignalBlocker blocker(m_actionDeformedToggle);
        m_actionDeformedToggle->setChecked(checked);
    }
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDeformedVisible(checked);
    }
}

void MainWindow::onActionToggleReactions(bool checked)
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setReactionsVisible(checked);
    }
}

void MainWindow::onActionDiagramMz()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::BendingMz);
    }
    if (m_diagramWidget)
    {
        m_diagramWidget->setDiagramType(TSA::Geometry::DiagramType::BendingMz);
    }
}

void MainWindow::onActionDiagramMy()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
    }
    if (m_diagramWidget)
    {
        m_diagramWidget->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
    }
}

void MainWindow::onActionDiagramMx()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::TorsionMx);
    }
    if (m_diagramWidget)
    {
        m_diagramWidget->setDiagramType(TSA::Geometry::DiagramType::TorsionMx);
    }
}

void MainWindow::onActionDiagramVz()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::ShearForceVz);
    }
    if (m_diagramWidget)
    {
        m_diagramWidget->setDiagramType(TSA::Geometry::DiagramType::ShearForceVz);
    }
}

void MainWindow::onActionDiagramVy()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::ShearForceVy);
    }
    if (m_diagramWidget)
    {
        m_diagramWidget->setDiagramType(TSA::Geometry::DiagramType::ShearForceVy);
    }
}

void MainWindow::onActionDiagramN()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::AxialForceN);
    }
    if (m_diagramWidget)
    {
        m_diagramWidget->setDiagramType(TSA::Geometry::DiagramType::AxialForceN);
    }
}

void MainWindow::onActionDiagramDeflection()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::DeflectionUz);
    }
    if (m_diagramWidget)
    {
        m_diagramWidget->setDiagramType(TSA::Geometry::DiagramType::DeflectionUz);
    }
}

void MainWindow::onActionDiagramNone()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::None);
    }
}

void MainWindow::onFitModel()
{
    if (m_occView) m_occView->fitModel();
}

void MainWindow::onFitResults()
{
    if (m_occView) m_occView->fitResults();
}

void MainWindow::onFitDeformed()
{
    if (m_occView) m_occView->fitDeformed();
}

void MainWindow::onActionResultsDisp()
{
    onActionToggleDeformed(true);
}

void MainWindow::onActionResultsForces()
{
    onActionDiagramMz();
}

void MainWindow::onActionResultsStress()
{
    onActionDiagramN();
}

void MainWindow::onActionMeasure()
{
    if (!m_model) return;

    int n1Id = -1, n2Id = -1;
    const auto selNodes = m_selectionManager ? m_selectionManager->selectedNodes() : std::set<int>{};

    if (selNodes.size() >= 2)
    {
        auto it = selNodes.begin();
        n1Id = *it++;
        n2Id = *it;
    }
    else
    {
        bool ok = false;
        QString text = QInputDialog::getText(this, tr("Mesure 3D"),
            tr("Entrez les ID des 2 nœuds à mesurer (ex: 1 2) :"),
            QLineEdit::Normal, "1 2", &ok);
        if (!ok || text.trimmed().isEmpty()) return;

        std::string s = text.toStdString();
        for (char& c : s) if (c == ',' || c == ';') c = ' ';
        std::istringstream iss(s);
        iss >> n1Id >> n2Id;
    }

    const auto* n1 = m_model->getNode(n1Id);
    const auto* n2 = m_model->getNode(n2Id);
    if (!n1 || !n2)
    {
        QMessageBox::warning(this, tr("Mesure 3D"), tr("Les nœuds spécifiés (%1, %2) n'existent pas.").arg(n1Id).arg(n2Id));
        return;
    }

    double dx = n2->x() - n1->x();
    double dy = n2->y() - n1->y();
    double dz = n2->z() - n1->z();
    double dist3d = std::sqrt(dx * dx + dy * dy + dz * dz);
    double dist2d = std::sqrt(dx * dx + dy * dy);
    double slope = dist2d > 1e-6 ? (std::abs(dz) / dist2d) * 100.0 : 90.0;
    double angleDeg = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("=== MESURE 3D ENTRE NŒUDS #%1 ET #%2 ===").arg(n1Id).arg(n2Id), "SYS");
        m_consoleDock->appendLog(tr("  Distance 3D directe : %1 m").arg(dist3d, 0, 'f', 4), "SUCCESS");
        m_consoleDock->appendLog(tr("  Distance Horizontale: %1 m").arg(dist2d, 0, 'f', 4), "INFO");
        m_consoleDock->appendLog(tr("  Delta X: %1 m | Delta Y: %2 m | Delta Z: %3 m").arg(dx, 0, 'f', 4).arg(dy, 0, 'f', 4).arg(dz, 0, 'f', 4), "INFO");
        m_consoleDock->appendLog(tr("  Pente: %1 % | Angle XY: %2 °").arg(slope, 0, 'f', 2).arg(angleDeg, 0, 'f', 2), "INFO");
    }

    QMessageBox::information(this, tr("Outil de Mesure 3D"),
        tr("Mesure entre Nœud #%1 (%2, %3, %4) et Nœud #%2 (%5, %6, %7) :\n\n"
           "• Distance 3D spatiale  : %8 m\n"
           "• Distance Horizontale : %9 m\n"
           "• ΔX = %10 m\n"
           "• ΔY = %11 m\n"
           "• ΔZ = %12 m\n"
           "• Pente / Inclinaison  : %13 % (%14°)")
        .arg(n1Id).arg(n1->x()).arg(n1->y()).arg(n1->z())
        .arg(n2Id).arg(n2->x()).arg(n2->y()).arg(n2->z())
        .arg(dist3d, 0, 'f', 4)
        .arg(dist2d, 0, 'f', 4)
        .arg(dx, 0, 'f', 4)
        .arg(dy, 0, 'f', 4)
        .arg(dz, 0, 'f', 4)
        .arg(slope, 0, 'f', 2)
        .arg(angleDeg, 0, 'f', 2));

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Mesure 3D : Distance = %1 m (ΔX=%2, ΔY=%3, ΔZ=%4)")
            .arg(dist3d, 0, 'f', 3).arg(dx, 0, 'f', 2).arg(dy, 0, 'f', 2).arg(dz, 0, 'f', 2));
    }
}
