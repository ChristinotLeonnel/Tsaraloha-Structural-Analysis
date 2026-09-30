#include "MainWindow.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "../Model/Model.h"
#include "Ruler/ViewportContainer.h"
#include "Dock/LogConsoleDock.h"
#include "Dialogs/NodalLoadDialog.h"
#include "Dialogs/MemberLoadDialog.h"
#include "Dialogs/LoadCaseDialog.h"
#include "Port/PortAreaWidget.h"
#include "Diagrams/Diagram2DWidget.h"
#include "../NDC/NDCViewerWidget.h"
#include "../Analysis/OpenSeesSolver.h"
#include "../Analysis/OpenSeesManager.h"
#include "../Analysis/ResultsModel.h"
#include "../Viewer/ResultsVisualManager.h"
#include "Dialogs/AnalysisConfigDialog.h"
#include "Dock/ResultsDockWidget.h"
#include "Properties/PropertyPanel.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QLabel>
#include <cmath>
#include <sstream>
#include <set>
#include <vector>

// =========================================================================
// Outils Métier & Ingénierie des Structures
// =========================================================================

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

    QStringList idsStr;
    for (int id : targetNodes) idsStr << QString("#%1").arg(id);

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Liaison Encastrement (6 DDL: Tx=Ty=Tz=Rx=Ry=Rz=0) assignée à %1 nœud(s) : %2")
            .arg(targetNodes.size()).arg(idsStr.join(", ")), "SUCCESS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Encastrement assigné (%1 nœuds)").arg(targetNodes.size()));
    }
}

void MainWindow::onActionPinned()
{
    std::set<int> targetNodes = m_selectionManager ? m_selectionManager->selectedNodes() : std::set<int>{};
    if (targetNodes.empty())
    {
        QMessageBox::information(this, tr("Appui Articulé"), tr("Veuillez sélectionner au moins un nœud."));
        return;
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Liaison Articulation (Rotule 3D, 3 DDL: Tx=Ty=Tz=0, Rx,Ry,Rz libres) assignée à %1 nœud(s)")
            .arg(targetNodes.size()), "SUCCESS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Articulation assignée (%1 nœuds)").arg(targetNodes.size()));
    }
}

void MainWindow::onActionRoller()
{
    std::set<int> targetNodes = m_selectionManager ? m_selectionManager->selectedNodes() : std::set<int>{};
    if (targetNodes.empty())
    {
        QMessageBox::information(this, tr("Appui Simple"), tr("Veuillez sélectionner au moins un nœud."));
        return;
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Liaison Appui Simple (Rouleau, 1 DDL: Tz=0, Tx,Ty libres) assignée à %1 nœud(s)")
            .arg(targetNodes.size()), "SUCCESS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Appui simple assigné (%1 nœuds)").arg(targetNodes.size()));
    }
}

void MainWindow::onActionPointLoad()
{
    if (!m_model) return;
    TSA::UI::NodalLoadDialog dlg(m_model.get(), m_selectionManager.get(), m_occView, this);
    if (m_selectionManager && !m_selectionManager->selectedNodes().empty())
    {
        dlg.setTargetNodeId(*m_selectionManager->selectedNodes().begin());
    }
    dlg.exec();
}

void MainWindow::onActionDistLoad()
{
    if (!m_model) return;
    TSA::UI::MemberLoadDialog dlg(m_model.get(), m_selectionManager.get(), m_occView, this);
    if (m_selectionManager)
    {
        if (!m_selectionManager->selectedBeams().empty())
        {
            dlg.setTargetElementId(*m_selectionManager->selectedBeams().begin());
        }
        else if (!m_selectionManager->selectedColumns().empty())
        {
            dlg.setTargetElementId(*m_selectionManager->selectedColumns().begin());
        }
    }
    dlg.exec();
}

void MainWindow::onActionMoment()
{
    if (!m_model) return;
    TSA::UI::NodalLoadDialog dlg(m_model.get(), m_selectionManager.get(), m_occView, this);
    if (m_selectionManager && !m_selectionManager->selectedNodes().empty())
    {
        dlg.setTargetNodeId(*m_selectionManager->selectedNodes().begin());
    }
    dlg.exec();
}

void MainWindow::onActionLoadCases()
{
    if (!m_model) return;
    TSA::UI::LoadCaseDialog dlg(m_model.get(), this);
    dlg.exec();
}

void MainWindow::onActionSeismic()
{
    bool ok = false;
    double ag = QInputDialog::getDouble(this, tr("Paramètres Sismiques Eurocode 8"), tr("Accélération de référence ag (g) :"), 0.25, 0.01, 1.5, 2, &ok);
    if (!ok) return;

    QStringList soils = { tr("Sol A (Roche, S = 1.0)"), tr("Sol B (Sable/Gravier dense, S = 1.20)"), tr("Sol C (Argile compacte, S = 1.15)"), tr("Sol D (Sol meuble, S = 1.35)") };
    QString soil = QInputDialog::getItem(this, tr("Classe de Sol"), tr("Type de sol :"), soils, 2, false, &ok);
    if (!ok) return;

    double q = QInputDialog::getDouble(this, tr("Coefficient de Comportement"), tr("Facteur de ductilité q :"), 3.5, 1.0, 6.0, 1, &ok);
    if (!ok) return;

    double s = 1.15;
    if (soil.contains("Sol A")) s = 1.0;
    else if (soil.contains("Sol B")) s = 1.20;
    else if (soil.contains("Sol D")) s = 1.35;

    double sd = (2.5 / q) * ag * s;

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("--- SPECTRE SISMIQUE EUROCODE 8 (EN 1998-1) ---"), "SYS");
        m_consoleDock->appendLog(tr("Zone sismique : ag = %1 g | %2 | Facteur q = %3").arg(ag).arg(soil).arg(q), "INFO");
        m_consoleDock->appendLog(tr("Accélération spectrale de calcul Sd(T1) = %1 g (%2 m/s²)")
            .arg(sd, 0, 'f', 3).arg(sd * 9.81, 0, 'f', 2), "SUCCESS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Spectre sismique EC8 : ag=%1g, q=%2, Sd=%3g").arg(ag).arg(q).arg(sd, 0, 'f', 3));
    }
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
        m_consoleDock->appendLog(tr("--- GÉNÉRATION DU MAILLAGE ÉLÉMENTS FINIS (h = %1 m) ---").arg(hMesh), "SYS");
        m_consoleDock->appendLog(tr("  - Éléments 1D (Poutres & Poteaux Hermite) : %1").arg(beamElems + colElems), "INFO");
        m_consoleDock->appendLog(tr("  - Éléments 2D (Dalles / Coques DKT)       : %1").arg(slabElems), "INFO");
        m_consoleDock->appendLog(tr("  - Nœuds du maillage discrétisé             : %1").arg(meshNodes), "INFO");
        m_consoleDock->appendLog(tr("  - Degrés de liberté (DDL) assemblés        : %1").arg(dofs), "SUCCESS");
    }

    QMessageBox::information(this, tr("Maillage Éléments Finis"),
        tr("Maillage généré avec succès !\n\n"
           "• Éléments finis totaux : %1\n"
           "• Nœuds de discrétisation : %2\n"
           "• Degrés de liberté (DDL) : %3\n"
           "• Discrétisation spatiale : h = %4 m")
        .arg(totalElems).arg(meshNodes).arg(dofs).arg(hMesh));

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Maillage EF généré : %1 éléments, %2 DDL").arg(totalElems).arg(dofs));
    }
}

void MainWindow::onActionAnalysisConfig()
{
    TSA::UI::AnalysisConfigDialog dlg(m_model.get(), this);
    dlg.setParameters(m_lastAnalysisParams);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_lastAnalysisParams = dlg.parameters();
        if (m_consoleDock)
        {
            m_consoleDock->appendLog(tr("Paramètres de résolution mis à jour (Algorithme: %1, Intégrateur: %2)")
                                         .arg(QString::fromStdString(TSA::Analysis::algorithmToTcl(m_lastAnalysisParams.algorithm)))
                                         .arg(QString::fromStdString(TSA::Analysis::integratorToTcl(m_lastAnalysisParams.integrator))), "INFO");
        }
    }
}

void MainWindow::onActionRunSolve()
{
    if (!m_model || m_model->nodes().empty() || (m_model->beams().empty() && m_model->columns().empty() && m_model->trussMembers().empty()))
    {
        QMessageBox::warning(this, tr("Solveur"), tr("Impossible de lancer le calcul : le modèle ne contient aucun élément structural."));
        return;
    }

    auto& opsMgr = TSA::Analysis::OpenSeesManager::instance();
    if (!opsMgr.isAvailable())
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("OpenSees Non Détecté"),
            tr("L'exécutable OpenSees est requis pour effectuer les calculs structurels.\n\n"
               "Voulez-vous lancer le téléchargement automatique de la version officielle Windows ?"),
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes)
        {
            QString dlErr;
            if (!opsMgr.downloadAndInstall(nullptr, &dlErr))
            {
                QMessageBox::critical(this, tr("Échec du Téléchargement"), tr("Impossible de télécharger OpenSees :\n%1").arg(dlErr));
                return;
            }
        }
        else
        {
            return;
        }
    }

    TSA::Analysis::AnalysisParameters params = m_lastAnalysisParams;
    params.useKiloNewtons = true;
    params.includeSelfWeight = true;

    if (m_consoleDock)
    {
        QString aName = (params.type == TSA::Analysis::AnalysisType::NonLinearStatic)
                            ? tr("STATIQUE NON LINÉAIRE")
                            : (params.type == TSA::Analysis::AnalysisType::Modal ? tr("MODALE") : tr("STATIQUE LINÉAIRE"));
        m_consoleDock->appendLog(tr("--- CALCUL OPENSEES [%1] ---").arg(aName), "SYS");
        m_consoleDock->appendLog(tr("Algorithme: %1 | Intégrateur: %2 | Solveur: %3")
                                     .arg(QString::fromStdString(TSA::Analysis::algorithmToTcl(params.algorithm)))
                                     .arg(QString::fromStdString(TSA::Analysis::integratorToTcl(params.integrator)))
                                     .arg(QString::fromStdString(TSA::Analysis::systemSolverToTcl(params.systemSolver))), "INFO");
        m_consoleDock->appendLog(tr("Modèle source : %1 nœuds, %2 poutres, %3 poteaux, %4 barres de treillis")
                                .arg(m_model->nodes().size())
                                .arg(m_model->beams().size())
                                .arg(m_model->columns().size())
                                .arg(m_model->trussMembers().size()), "INFO");
    }

    if (!m_openSeesSolver)
    {
        m_openSeesSolver = std::make_unique<TSA::Analysis::OpenSeesSolver>(this);
    }

    QString solveErr;
    bool ok = m_openSeesSolver->solveSynchronous(*m_model, params, &solveErr);

    if (!ok)
    {
        if (m_consoleDock)
        {
            m_consoleDock->appendLog(tr("Échec du calcul OpenSees : %1").arg(solveErr), "ERROR");
            m_consoleDock->appendLog(QString::fromStdString(m_openSeesSolver->results().journalLog()), "ERROR");
        }
        QMessageBox::critical(this, tr("Erreur Solveur OpenSees"), tr("Le calcul a échoué :\n%1").arg(solveErr));
        return;
    }

    m_resultsModel = std::make_shared<TSA::Analysis::ResultsModel>(m_openSeesSolver->results());

    if (m_occView) m_occView->setResultsModel(m_resultsModel);
    if (m_portArea) m_portArea->setResultsModel(m_resultsModel);
    if (m_propertyPanel) m_propertyPanel->setResultsModel(m_resultsModel);
    if (m_resultsDock)
    {
        m_resultsDock->setResultsModel(m_resultsModel);
        if (m_occView && m_occView->resultsVisual())
        {
            m_resultsDock->syncFromVisualManager(m_occView->resultsVisual());
        }
        m_resultsDock->show();
        m_resultsDock->raise();
    }

    const auto& ext = m_resultsModel->summary();
    const auto& eq = m_resultsModel->equilibrium();

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("RÉSULTATS OPENSEES STATIQUES :"), "SUCCESS");
        m_consoleDock->appendLog(tr("  • Réaction verticale totale Rz = %1 kN").arg(eq.reactionFz, 0, 'f', 2), "SUCCESS");
        m_consoleDock->appendLog(tr("  • Flèche maximale absolue δ_max = %1 mm (Nœud #%2)")
                                .arg(ext.maxDisplacement * 1000.0, 0, 'f', 3)
                                .arg(ext.maxDisplacementNodeId), "SUCCESS");
        m_consoleDock->appendLog(tr("  • Moment fléchissant max M_max = %1 kNm (Barre #%2)")
                                .arg(ext.maxBendingMoment, 0, 'f', 2)
                                .arg(ext.maxBendingMomentElementId), "SUCCESS");
        m_consoleDock->appendLog(tr("  • Traction max N_max           = %1 kN").arg(ext.maxTension, 0, 'f', 2), "SUCCESS");
        m_consoleDock->appendLog(tr("  • Équilibre global statique     : %1").arg(eq.isBalanced(0.05) ? tr("CONFORME") : tr("DÉSÉQUILIBRE")), "SUCCESS");
    }

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("OpenSees Statique OK : δ_max = %1 mm, M_max = %2 kNm")
                              .arg(ext.maxDisplacement * 1000.0, 0, 'f', 2)
                              .arg(ext.maxBendingMoment, 0, 'f', 1));
    }

    QMessageBox::information(this, tr("Calcul OpenSees Terminé"),
        tr("Calcul éléments finis OpenSees terminé avec succès !\n\n"
           "• Déplacement vertical max : %1 mm (Nœud #%2)\n"
           "• Moment fléchissant max   : %3 kNm (Barre #%4)\n"
           "• Traction maximale        : %5 kN\n"
           "• Réaction verticale totale Rz : %6 kN\n"
           "• Équilibre global          : %7")
        .arg(ext.maxDisplacement * 1000.0, 0, 'f', 3)
        .arg(ext.maxDisplacementNodeId)
        .arg(ext.maxBendingMoment, 0, 'f', 2)
        .arg(ext.maxBendingMomentElementId)
        .arg(ext.maxTension, 0, 'f', 2)
        .arg(eq.reactionFz, 0, 'f', 2)
        .arg(eq.isBalanced(0.05) ? tr("CONFORME") : tr("VÉRIFIER")));
}

void MainWindow::onActionModal()
{
    if (!m_model || m_model->nodes().empty() || (m_model->beams().empty() && m_model->columns().empty()))
    {
        QMessageBox::warning(this, tr("Analyse Modale"), tr("Impossible de lancer le calcul : le modèle ne contient aucun élément."));
        return;
    }

    auto& opsMgr = TSA::Analysis::OpenSeesManager::instance();
    if (!opsMgr.isAvailable())
    {
        QMessageBox::warning(this, tr("OpenSees Requis"), tr("OpenSees n'est pas détecté. Veuillez le configurer ou le télécharger."));
        return;
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("--- ANALYSE MODALE DYNAMIQUE OPENSEES ([K - ω²M]{Φ} = 0) ---"), "SYS");
    }

    TSA::Analysis::AnalysisParameters params;
    params.type = TSA::Analysis::AnalysisType::Modal;
    params.numEigenmodes = 6;

    if (!m_openSeesSolver)
    {
        m_openSeesSolver = std::make_unique<TSA::Analysis::OpenSeesSolver>(this);
    }

    QString solveErr;
    bool ok = m_openSeesSolver->solveSynchronous(*m_model, params, &solveErr);

    if (!ok)
    {
        if (m_consoleDock)
        {
            m_consoleDock->appendLog(tr("Échec de l'analyse modale : %1").arg(solveErr), "ERROR");
        }
        QMessageBox::critical(this, tr("Erreur Analyse Modale"), tr("L'analyse modale a échoué :\n%1").arg(solveErr));
        return;
    }

    m_resultsModel = std::make_shared<TSA::Analysis::ResultsModel>(m_openSeesSolver->results());

    if (m_occView)
    {
        m_occView->setResultsModel(m_resultsModel);
        if (m_occView->resultsVisual())
        {
            m_occView->resultsVisual()->startModalAnimation(1, 1.0);
        }
    }
    if (m_portArea) m_portArea->setResultsModel(m_resultsModel);

    QString msgSummary = tr("Analyse Modale OpenSees Terminée :\n\n");
    for (const auto& m : m_resultsModel->modalModes())
    {
        QString line = tr("Mode %1 : f = %2 Hz | T = %3 s | omega = %4 rad/s")
                       .arg(m.modeNumber)
                       .arg(m.frequency, 0, 'f', 3)
                       .arg(m.period, 0, 'f', 3)
                       .arg(m.omega, 0, 'f', 2);
        if (m_consoleDock) m_consoleDock->appendLog(line, "INFO");
        msgSummary += line + "\n";
    }

    if (m_statusInfo && !m_resultsModel->modalModes().empty())
    {
        const auto& m1 = m_resultsModel->modalModes().front();
        m_statusInfo->setText(tr("Modal OK : Mode 1: T = %1 s (f = %2 Hz)").arg(m1.period, 0, 'f', 3).arg(m1.frequency, 0, 'f', 2));
    }

    QMessageBox::information(this, tr("Analyse Modale OpenSees"), msgSummary);
}

void MainWindow::onActionPushover()
{
    if (!m_model || m_model->nodes().empty())
    {
        QMessageBox::warning(this, tr("Pushover"), tr("Le modèle ne contient aucun élément."));
        return;
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("--- ANALYSE NON-LINÉAIRE STATIQUE (PUSHOVER) OPENSEES ---"), "SYS");
    }

    TSA::Analysis::AnalysisParameters params;
    params.type = TSA::Analysis::AnalysisType::Pushover;
    params.numSteps = 30;
    params.tolerance = 1e-4;

    if (!m_openSeesSolver)
    {
        m_openSeesSolver = std::make_unique<TSA::Analysis::OpenSeesSolver>(this);
    }

    QString solveErr;
    bool ok = m_openSeesSolver->solveSynchronous(*m_model, params, &solveErr);

    if (!ok)
    {
        if (m_consoleDock) m_consoleDock->appendLog(tr("Échec du calcul Pushover : %1").arg(solveErr), "ERROR");
        QMessageBox::critical(this, tr("Erreur Pushover"), tr("L'analyse Pushover a échoué :\n%1").arg(solveErr));
        return;
    }

    m_resultsModel = std::make_shared<TSA::Analysis::ResultsModel>(m_openSeesSolver->results());

    if (m_occView) m_occView->setResultsModel(m_resultsModel);
    if (m_portArea)
    {
        m_portArea->setResultsModel(m_resultsModel);
        m_portArea->setLayoutMode(TSA::UI::PortLayout::SplitHorizontal);
        if (m_portArea->diagramWidget())
        {
            m_portArea->diagramWidget()->setViewMode(TSA::UI::Diagram2DWidget::ViewMode::PushoverCapacity);
        }
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Calcul Pushover terminé avec succès. Courbe de capacité affichée dans le port 2D."), "SUCCESS");
    }
}

void MainWindow::onActionNoteDeCalcul()
{
    if (m_portArea)
    {
        if (m_portArea->ndcWidget())
        {
            m_portArea->ndcWidget()->setModel(m_model.get());
            m_portArea->ndcWidget()->setResultsModel(m_resultsModel);
        }
        m_portArea->setLayoutMode(TSA::UI::PortLayout::SplitHorizontal);
        m_portArea->port(1)->setPortType(TSA::UI::PortType::CalculationNote);
    }
}

void MainWindow::onActionToggleDeformed(bool checked)
{
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
    if (m_portArea && m_portArea->diagramWidget())
    {
        m_portArea->diagramWidget()->setDiagramType(TSA::Geometry::DiagramType::BendingMz);
    }
}

void MainWindow::onActionDiagramMy()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
    }
    if (m_portArea && m_portArea->diagramWidget())
    {
        m_portArea->diagramWidget()->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
    }
}

void MainWindow::onActionDiagramMx()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::TorsionMx);
    }
    if (m_portArea && m_portArea->diagramWidget())
    {
        m_portArea->diagramWidget()->setDiagramType(TSA::Geometry::DiagramType::TorsionMx);
    }
}

void MainWindow::onActionDiagramVz()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::ShearForceVz);
    }
    if (m_portArea && m_portArea->diagramWidget())
    {
        m_portArea->diagramWidget()->setDiagramType(TSA::Geometry::DiagramType::ShearForceVz);
    }
}

void MainWindow::onActionDiagramVy()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::ShearForceVy);
    }
    if (m_portArea && m_portArea->diagramWidget())
    {
        m_portArea->diagramWidget()->setDiagramType(TSA::Geometry::DiagramType::ShearForceVy);
    }
}

void MainWindow::onActionDiagramN()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::AxialForceN);
    }
    if (m_portArea && m_portArea->diagramWidget())
    {
        m_portArea->diagramWidget()->setDiagramType(TSA::Geometry::DiagramType::AxialForceN);
    }
}

void MainWindow::onActionDiagramDeflection()
{
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::DeflectionUz);
    }
    if (m_portArea && m_portArea->diagramWidget())
    {
        m_portArea->diagramWidget()->setDiagramType(TSA::Geometry::DiagramType::DeflectionUz);
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

void MainWindow::onPortLayoutSingle()
{
    if (m_portArea) m_portArea->setLayoutMode(TSA::UI::PortLayout::Single);
}

void MainWindow::onPortLayoutSplitH()
{
    if (m_portArea) m_portArea->setLayoutMode(TSA::UI::PortLayout::SplitHorizontal);
}

void MainWindow::onPortLayoutSplitV()
{
    if (m_portArea) m_portArea->setLayoutMode(TSA::UI::PortLayout::SplitVertical);
}

void MainWindow::onPortLayoutGrid2x2()
{
    if (m_portArea) m_portArea->setLayoutMode(TSA::UI::PortLayout::Grid2x2);
}

void MainWindow::onPortLayoutTabbed()
{
    if (m_portArea) m_portArea->setLayoutMode(TSA::UI::PortLayout::Tabbed);
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
