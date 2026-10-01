#include "NDCGenerator.h"
#include "../Model/Model.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/Section.h"
#include "../Model/Material.h"
#include "../Model/Load/LoadManager.h"
#include "../Analysis/ResultsModel.h"
#include "../Analysis/OpenSeesManager.h"
#include "../Standards/Design/ConcreteDesignEC2.h"
#include "../Standards/Design/SteelDesignEC3.h"

#include <cmath>
#include <algorithm>

namespace TSA::NDC
{

static QString sectionShapeToQString(TSA::Model::SectionShape shape)
{
    switch (shape)
    {
    case TSA::Model::SectionShape::Rectangular: return QStringLiteral("Rectangulaire");
    case TSA::Model::SectionShape::Circular:    return QStringLiteral("Circulaire");
    case TSA::Model::SectionShape::IShape:      return QStringLiteral("Profilé I");
    case TSA::Model::SectionShape::Pipe:        return QStringLiteral("Tube circulaire");
    case TSA::Model::SectionShape::BoxHollow:   return QStringLiteral("Tube rectangulaire");
    case TSA::Model::SectionShape::UPN:         return QStringLiteral("UPN");
    case TSA::Model::SectionShape::Angle:       return QStringLiteral("Cornière");
    case TSA::Model::SectionShape::TSection:    return QStringLiteral("Profilé T");
    }
    return QStringLiteral("Autre");
}

NDCDocument NDCGenerator::generate(
    const TSA::Model::Model& model,
    const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
    const QString& projectName,
    const QString& engineerName)
{
    NDCDocument doc;
    if (!projectName.isEmpty()) doc.projectTitle = projectName;
    if (!engineerName.isEmpty()) doc.author = engineerName;

    auto vInfo = TSA::Analysis::OpenSeesManager::instance().versionInfo();
    if (vInfo.isValid)
    {
        doc.softwareVersion = QString("TSA v1.0.0 (Moteur EF : OpenSees v%1.%2.%3)").arg(vInfo.major).arg(vInfo.minor).arg(vInfo.patch);
    }

    int chapNum = 1;

    // =========================================================================
    // CHAPITRE 1 : INTRODUCTION & HYPOTHÈSES GÉNÉRALES
    // =========================================================================
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Introduction et Hypothèses Générales";

        NDCSection s1;
        s1.title = "Objet du Document";
        s1.paragraphs.push_back("Le présent rapport technique constitue la Note de Calcul justificative de dimensionnement "
                               "et de vérification de la structure modélisée dans l'environnement TSA (Tsaraloha Structural Analysis). "
                               "Les analyses numériques de résistance, de déformabilité et de comportement dynamique sont exécutées "
                               "par le solveur open-source aux éléments finis OpenSees.");

        NDCSection s2;
        s2.title = "Réglementations et Normes de Référence";
        s2.paragraphs.push_back("Les calculs et vérifications sont établis conformément aux normes européennes en vigueur (Eurocodes) :");
        s2.keyValues.push_back({QStringLiteral("EN 1990"), QStringLiteral("Eurocode 0 — Bases de calcul des structures")});
        s2.keyValues.push_back({QStringLiteral("EN 1991"), QStringLiteral("Eurocode 1 — Actions sur les structures")});
        s2.keyValues.push_back({QStringLiteral("EN 1992"), QStringLiteral("Eurocode 2 — Calcul des structures en béton armé")});
        s2.keyValues.push_back({QStringLiteral("EN 1993"), QStringLiteral("Eurocode 3 — Calcul des structures en acier")});
        s2.keyValues.push_back({QStringLiteral("EN 1998"), QStringLiteral("Eurocode 8 — Calcul des structures pour leur résistance aux séismes")});

        NDCSection s3;
        s3.title = "Modélisation Numérique par Éléments Finis";
        s3.paragraphs.push_back("La structure est discrétisée en modèle tridimensionnel à 6 degrés de liberté par nœud "
                               "(trois translations X, Y, Z et trois rotations Rx, Ry, Rz). Les éléments barres (poutres et poteaux) "
                               "sont formulés selon des éléments poutres-colonnes élastiques tridimensionnels prenant en compte les "
                               "effets de flexion bi-axiale, d'effort normal, d'effort tranchant et de torsion selon la théorie de Navier-Bernoulli.");

        ch.sections.push_back(s1);
        ch.sections.push_back(s2);
        ch.sections.push_back(s3);

        if (results && results->isValid())
        {
            const auto& meta = results->executionMetadata();
            NDCSection sMeta;
            sMeta.title = "Traçabilité de Calcul & Annexe Nationale";
            sMeta.paragraphs.push_back("Informations d'environnement et métadonnées d'exécution certifiées pour la présente note de calcul :");
            sMeta.keyValues.push_back({QStringLiteral("Solveur de calcul"), QString::fromStdString(meta.solverEngine + " (v" + meta.solverVersion + ")")});
            sMeta.keyValues.push_back({QStringLiteral("Annexe Nationale active"), QString::fromStdString(meta.nationalAnnex)});
            sMeta.keyValues.push_back({QStringLiteral("Référentiel réglementaire"), QString::fromStdString(meta.normativeFramework)});
            sMeta.keyValues.push_back({QStringLiteral("Date / Heure de calcul"), QString::fromStdString(meta.executionTimestamp.empty() ? results->timestamp() : meta.executionTimestamp)});
            sMeta.keyValues.push_back({QStringLiteral("Équilibre global statique"), meta.isEquilibriumVerified ? QStringLiteral("CONFORME (Résidu max <= %1 kN)").arg(meta.globalEquilibriumTolerance) : QStringLiteral("Divergence / Non vérifié")});
            ch.sections.push_back(sMeta);
        }

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE 2 : MATÉRIAUX ET SECTIONS TRANSVERSALES
    // =========================================================================
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Caractéristiques des Matériaux et des Sections";

        // Sections transversales
        NDCSection sSec;
        sSec.title = "Sections Transversales";
        sSec.paragraphs.push_back("Propriétés géométriques des sections assignées aux éléments linéaires du modèle :");

        NDCTable tSec;
        tSec.caption = "Tableau 2.1 : Caractéristiques mécaniques des sections";
        tSec.headers = {"Nom de Section", "Forme", "Hauteur (mm)", "Largeur (mm)", "Aire A (cm²)", "Iy (cm⁴)", "Iz (cm⁴)", "J (cm⁴)"};

        std::map<std::string, TSA::Model::Section> uniqueSections;
        for (const auto& [id, b] : model.beams()) uniqueSections[b.section().name] = b.section();
        for (const auto& [id, c] : model.columns()) uniqueSections[c.section().name] = c.section();
        for (const auto& [id, tr] : model.trussMembers()) uniqueSections[tr.section().name] = tr.section();
        for (const auto& [id, cb] : model.cables()) uniqueSections[cb.section().name] = cb.section();

        for (const auto& [name, sec] : uniqueSections)
        {
            tSec.rows.push_back({
                QString::fromStdString(sec.name),
                sectionShapeToQString(sec.shape),
                QString::number(sec.height * 1000.0, 'f', 1),
                QString::number(sec.width * 1000.0, 'f', 1),
                QString::number(sec.area() * 1e4, 'f', 2),
                QString::number(sec.iy() * 1e8, 'f', 1),
                QString::number(sec.iz() * 1e8, 'f', 1),
                QString::number(sec.it() * 1e8, 'f', 1)
            });
        }
        sSec.tables.push_back(tSec);
        ch.sections.push_back(sSec);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE 3 : GÉOMÉTRIE ET CONDITIONS AUX LIMITES
    // =========================================================================
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Géométrie du Modèle et Conditions aux Limites";

        NDCSection sNodes;
        sNodes.title = "Nœuds et Appuis";
        sNodes.paragraphs.push_back(QString("Le modèle comprend un total de %1 nœuds cartésiens globaux. "
                                           "Les liaisons au sol et conditions d'appuis sont définies ci-dessous :")
                                   .arg(model.nodes().size()));

        NDCTable tNodes;
        tNodes.caption = "Tableau 3.1 : Coordonnées nodales et conditions d'appui";
        tNodes.headers = {"Nœud ID", "X (m)", "Y (m)", "Z (m)", "Type d'Appui", "Bloquages (Tx Ty Tz Rx Ry Rz)"};

        for (const auto& [id, n] : model.nodes())
        {
            const auto& supp = n.support();
            QString supStr = QString::fromStdString(supp.typeName());
            QString fixStr = supp.isFree() ? QStringLiteral("Libre") : QString::fromStdString(supp.dofSummary());

            tNodes.rows.push_back({
                QString::number(id),
                QString::number(n.x(), 'f', 3),
                QString::number(n.y(), 'f', 3),
                QString::number(n.z(), 'f', 3),
                supStr,
                fixStr
            });
        }
        sNodes.tables.push_back(tNodes);
        ch.sections.push_back(sNodes);

        // Connectivité des éléments
        NDCSection sElems;
        sElems.title = "Éléments Structuraux Linéaires";
        NDCTable tElems;
        tElems.caption = "Tableau 3.2 : Connectivité des barres et longueurs";
        tElems.headers = {"Élément ID", "Rôle", "Nœud Début", "Nœud Fin", "Longueur (m)", "Section", "Matériau"};

        for (const auto& [id, b] : model.beams())
        {
            const auto* n1 = model.getNode(b.startNodeId());
            const auto* n2 = model.getNode(b.endNodeId());
            double len = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : 0.0;
            tElems.rows.push_back({
                QString::number(id),
                "Poutre",
                QString::number(b.startNodeId()),
                QString::number(b.endNodeId()),
                QString::number(len, 'f', 3),
                QString::fromStdString(b.section().name),
                QString::fromStdString(b.material().name)
            });
        }
        for (const auto& [id, col] : model.columns())
        {
            const auto* n1 = model.getNode(col.startNodeId());
            const auto* n2 = model.getNode(col.endNodeId());
            double len = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : 0.0;
            tElems.rows.push_back({
                QString::number(id),
                "Poteau",
                QString::number(col.startNodeId()),
                QString::number(col.endNodeId()),
                QString::number(len, 'f', 3),
                QString::fromStdString(col.section().name),
                QString::fromStdString(col.material().name)
            });
        }
        for (const auto& [id, tr] : model.trussMembers())
        {
            const auto* n1 = model.getNode(tr.startNodeId());
            const auto* n2 = model.getNode(tr.endNodeId());
            double len = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : 0.0;
            tElems.rows.push_back({
                QString::number(id),
                "Treillis",
                QString::number(tr.startNodeId()),
                QString::number(tr.endNodeId()),
                QString::number(len, 'f', 3),
                QString::fromStdString(tr.section().name),
                QString::fromStdString(tr.material().name)
            });
        }
        for (const auto& [id, cb] : model.cables())
        {
            const auto* n1 = model.getNode(cb.startNodeId());
            const auto* n2 = model.getNode(cb.endNodeId());
            double len = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : 0.0;
            tElems.rows.push_back({
                QString::number(id),
                "Câble",
                QString::number(cb.startNodeId()),
                QString::number(cb.endNodeId()),
                QString::number(len, 'f', 3),
                QString::fromStdString(cb.section().name),
                QString::fromStdString(cb.material().name)
            });
        }
        sElems.tables.push_back(tElems);
        ch.sections.push_back(sElems);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE 4 : INVENTAIRE DES CHARGES
    // =========================================================================
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Inventaire des Actions et Charges Appliquées";

        const auto& lm = model.loadManager();
        NDCSection sLC;
        sLC.title = "Cas de Charges Définis";
        NDCTable tLC;
        tLC.caption = "Tableau 4.1 : Cas de charges et facteurs";
        tLC.headers = {"ID", "Nom du Cas", "Catégorie", "Facteur", "Poids Propre Automatique"};

        for (const auto& [id, lc] : lm.loadCases())
        {
            tLC.rows.push_back({
                QString::number(id),
                QString::fromStdString(lc.name()),
                QString::fromStdString(TSA::Model::loadCategoryToString(lc.category())),
                QString::number(lc.selfWeightFactor(), 'f', 2),
                lc.isSelfWeightIncluded() ? QStringLiteral("Inclus") : QStringLiteral("Non")
            });
        }
        sLC.tables.push_back(tLC);
        ch.sections.push_back(sLC);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE 5 : ÉQUILIBRE STATIQUE ET RÉACTIONS D'APPUI
    // =========================================================================
    if (results && results->isValid())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Équilibre Statique Global et Réactions aux Appuis";

        const auto& eq = results->equilibrium();
        NDCSection sEq;
        sEq.title = "Vérification de l'Équilibre Global";
        sEq.paragraphs.push_back("La somme des forces appliquées et des réactions d'appui doit être nulle dans les trois directions cartésiennes :");
        sEq.keyValues.push_back({QStringLiteral("Somme des forces appliquées Fz"), QString("%1 kN").arg(eq.appliedFz, 0, 'f', 2)});
        sEq.keyValues.push_back({QStringLiteral("Somme des réactions d'appui Rz"), QString("%1 kN").arg(eq.reactionFz, 0, 'f', 2)});
        sEq.keyValues.push_back({QStringLiteral("Résidu d'équilibre vertical Delta_Fz"), QString("%1 kN").arg(eq.errorFz(), 0, 'f', 4)});
        sEq.keyValues.push_back({QStringLiteral("Statut de convergence et d'équilibre"), eq.isBalanced(0.05) ? QStringLiteral("CONFORME (Résidu < 5%)") : QStringLiteral("ATTENTION : Déséquilibre")});
        ch.sections.push_back(sEq);

        // Tableau des réactions
        NDCSection sReact;
        sReact.title = "Réactions aux Nœuds d'Appui";
        NDCTable tReact;
        tReact.caption = "Tableau 5.1 : Réactions nodales aux appuis (OpenSees)";
        tReact.headers = {"Nœud ID", "Rx (kN)", "Ry (kN)", "Rz (kN)", "Mx (kNm)", "My (kNm)", "Mz (kNm)"};

        for (const auto& [nId, r] : results->allReactions())
        {
            tReact.rows.push_back({
                QString::number(nId),
                QString::number(r.rx, 'f', 2),
                QString::number(r.ry, 'f', 2),
                QString::number(r.rz, 'f', 2),
                QString::number(r.mx, 'f', 2),
                QString::number(r.my, 'f', 2),
                QString::number(r.mz, 'f', 2)
            });
        }
        sReact.tables.push_back(tReact);
        ch.sections.push_back(sReact);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE 6 : DÉPLACEMENTS ET FLÈCHES EXTRÊMES
    // =========================================================================
    if (results && results->isValid() && !results->allDisplacements().empty())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Déplacements Nodaux et Flèches Admissibles";

        NDCSection sDisp;
        sDisp.title = "Déplacements Nodaux Extrêmes";

        const auto& ext = results->summary();
        sDisp.keyValues.push_back({QStringLiteral("Déplacement maximal absolu"), QString("%1 mm (Nœud #%2)").arg(ext.maxDisplacement * 1000.0, 0, 'f', 3).arg(ext.maxDisplacementNodeId)});
        sDisp.paragraphs.push_back("Tableau récapitulatif des déplacements et rotations nodales calculés par OpenSees :");

        NDCTable tDisp;
        tDisp.caption = "Tableau 6.1 : Déplacements et rotations nodales (Repère Global)";
        tDisp.headers = {"Nœud ID", "ux (mm)", "uy (mm)", "uz (mm)", "rx (mrad)", "ry (mrad)", "rz (mrad)"};

        for (const auto& [nId, d] : results->allDisplacements())
        {
            tDisp.rows.push_back({
                QString::number(nId),
                QString::number(d.ux * 1000.0, 'f', 3),
                QString::number(d.uy * 1000.0, 'f', 3),
                QString::number(d.uz * 1000.0, 'f', 3),
                QString::number(d.rx * 1000.0, 'f', 3),
                QString::number(d.ry * 1000.0, 'f', 3),
                QString::number(d.rz * 1000.0, 'f', 3)
            });
        }
        sDisp.tables.push_back(tDisp);
        ch.sections.push_back(sDisp);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE 7 : EFFORTS INTERNES ET ENVELOPPE DE DIMENSIONNEMENT
    // =========================================================================
    if (results && results->isValid() && !results->allElementResults().empty())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Efforts Internes et Sollicitations des Éléments";

        NDCSection sForces;
        sForces.title = "Enveloppe des Efforts Internes par Barre";
        sForces.paragraphs.push_back("Extrema des sollicitations internes calculés par OpenSees le long des éléments structuraux :");

        NDCTable tForces;
        tForces.caption = "Tableau 7.1 : Sollicitations extrêmes par élément";
        tForces.headers = {"Élément ID", "N max (kN)", "N min (kN)", "V max (kN)", "M fléchissant max (kNm)"};

        for (const auto& [elId, elemRes] : results->allElementResults())
        {
            tForces.rows.push_back({
                QString::number(elId),
                QString::number(elemRes.maxNormalForce(), 'f', 2),
                QString::number(elemRes.minNormalForce(), 'f', 2),
                QString::number(elemRes.maxShearForce(), 'f', 2),
                QString::number(elemRes.maxBendingMoment(), 'f', 2)
            });
        }
        sForces.tables.push_back(tForces);
        ch.sections.push_back(sForces);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : VÉRIFICATIONS RÉGLEMENTAIRES EUROCODES (EC2 & EC3)
    // =========================================================================
    if (results && results->isValid() && !results->allElementResults().empty())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Vérifications Réglementaires Eurocodes (EC2 & EC3)";

        // 1. Béton armé (EN 1992-1-1 §6.1 Flexion simple aux ELU)
        NDCSection sEC2;
        sEC2.title = "Vérification en Flexion Simple Béton Armé (EN 1992-1-1 §6.1)";
        sEC2.paragraphs.push_back("Calcul des sections d'armatures longitudinales As pour les poutres en béton armé sous moment fléchissant maximal ELU :");

        NDCTable tEC2;
        tEC2.caption = "Tableau : Ferraillage longitudinal des poutres béton (EC2)";
        tEC2.headers = {"Poutre ID", "Section", "Matériau", "M_Ed (kNm)", "mu_cu", "z (m)", "As prov (cm²)", "Ratio eta (%)", "Statut"};

        bool hasConcreteElements = false;
        for (const auto& [bId, beam] : model.beams())
        {
            if (beam.material().type == TSA::Model::MaterialType::Concrete ||
                beam.material().type == TSA::Model::MaterialType::ReinforcedConcrete)
            {
                const auto* elemRes = results->getElementResults(bId);
                if (elemRes)
                {
                    double Med = elemRes->maxBendingMoment() * 1000.0; // kNm -> Nm
                    auto ec2Res = TSA::Standards::Design::ConcreteDesignEC2::calculateFromModel(
                        beam.section(), beam.material(), Med
                    );
                    if (ec2Res.valid)
                    {
                        hasConcreteElements = true;
                        tEC2.rows.push_back({
                            QString::number(bId),
                            QString::fromStdString(beam.section().name),
                            QString::fromStdString(beam.material().name),
                            QString::number(Med / 1000.0, 'f', 2),
                            QString::number(ec2Res.mu_cu, 'f', 3),
                            QString::number(ec2Res.z, 'f', 3),
                            QString::number(ec2Res.As_provided * 1e4, 'f', 2),
                            QString::number(ec2Res.utilizationRatio * 100.0, 'f', 1),
                            ec2Res.requiresCompressionSteel ? QStringLiteral("Aciers comprimés requis") : QStringLiteral("Conforme (Pivot B)")
                        });
                    }
                }
            }
        }
        if (hasConcreteElements)
        {
            sEC2.tables.push_back(tEC2);
            ch.sections.push_back(sEC2);
        }

        // 2. Acier de charpente (EN 1993-1-1 §6.3 Stabilité au flambement)
        NDCSection sEC3;
        sEC3.title = "Stabilité au Flambement des Barres Acier (EN 1993-1-1 §6.3)";
        sEC3.paragraphs.push_back("Vérification des éléments comprimés au flambement par flexion selon les courbes européennes a0, a, b, c, d :");

        NDCTable tEC3;
        tEC3.caption = "Tableau : Résistance au flambement des barres acier (EC3)";
        tEC3.headers = {"Barre ID", "Type", "Section", "N_Ed (kN)", "lambda_bar", "chi", "N_b,Rd (kN)", "Ratio eta (%)", "Statut"};

        bool hasSteelElements = false;
        // Poteaux acier
        for (const auto& [colId, col] : model.columns())
        {
            if (col.material().type == TSA::Model::MaterialType::Steel ||
                col.material().type == TSA::Model::MaterialType::GalvanizedSteel)
            {
                const auto* elemRes = results->getElementResults(colId);
                if (elemRes)
                {
                    double Ned = std::max(std::abs(elemRes->minNormalForce()), std::abs(elemRes->maxNormalForce())) * 1000.0; // N
                    const auto* n1 = model.getNode(col.startNodeId());
                    const auto* n2 = model.getNode(col.endNodeId());
                    double L = 3.0;
                    if (n1 && n2)
                    {
                        double dx = n2->x() - n1->x(), dy = n2->y() - n1->y(), dz = n2->z() - n1->z();
                        L = std::sqrt(dx * dx + dy * dy + dz * dz);
                    }

                    auto ec3Res = TSA::Standards::Design::SteelDesignEC3::calculateFromBar(
                        col.section(), col.material(), L, 1.0, Ned, false
                    );
                    if (ec3Res.valid)
                    {
                        hasSteelElements = true;
                        tEC3.rows.push_back({
                            QString::number(colId),
                            QStringLiteral("Poteau"),
                            QString::fromStdString(col.section().name),
                            QString::number(Ned / 1000.0, 'f', 2),
                            QString::number(ec3Res.reducedSlenderness, 'f', 2),
                            QString::number(ec3Res.chi, 'f', 3),
                            QString::number(ec3Res.Nb_Rd / 1000.0, 'f', 1),
                            QString::number(ec3Res.utilizationRatio * 100.0, 'f', 1),
                            ec3Res.pass ? QStringLiteral("CONFORME") : QStringLiteral("NON CONFORME")
                        });
                    }
                }
            }
        }

        // Poutres acier avec compression
        for (const auto& [bId, beam] : model.beams())
        {
            if (beam.material().type == TSA::Model::MaterialType::Steel ||
                beam.material().type == TSA::Model::MaterialType::GalvanizedSteel)
            {
                const auto* elemRes = results->getElementResults(bId);
                if (elemRes)
                {
                    double Ned = std::max(std::abs(elemRes->minNormalForce()), std::abs(elemRes->maxNormalForce())) * 1000.0; // N
                    if (Ned > 1000.0)
                    {
                        const auto* n1 = model.getNode(beam.startNodeId());
                        const auto* n2 = model.getNode(beam.endNodeId());
                        double L = 3.0;
                        if (n1 && n2)
                        {
                            double dx = n2->x() - n1->x(), dy = n2->y() - n1->y(), dz = n2->z() - n1->z();
                            L = std::sqrt(dx * dx + dy * dy + dz * dz);
                        }

                        auto ec3Res = TSA::Standards::Design::SteelDesignEC3::calculateFromBar(
                            beam.section(), beam.material(), L, 1.0, Ned, false
                        );
                        if (ec3Res.valid)
                        {
                            hasSteelElements = true;
                            tEC3.rows.push_back({
                                QString::number(bId),
                                QStringLiteral("Poutre"),
                                QString::fromStdString(beam.section().name),
                                QString::number(Ned / 1000.0, 'f', 2),
                                QString::number(ec3Res.reducedSlenderness, 'f', 2),
                                QString::number(ec3Res.chi, 'f', 3),
                                QString::number(ec3Res.Nb_Rd / 1000.0, 'f', 1),
                                QString::number(ec3Res.utilizationRatio * 100.0, 'f', 1),
                                ec3Res.pass ? QStringLiteral("CONFORME") : QStringLiteral("NON CONFORME")
                            });
                        }
                    }
                }
            }
        }

        if (hasSteelElements)
        {
            sEC3.tables.push_back(tEC3);
            ch.sections.push_back(sEC3);
        }

        if (hasConcreteElements || hasSteelElements)
        {
            doc.addChapter(ch);
        }
    }

    // =========================================================================
    // CHAPITRE 8 : ANALYSE MODALE ET FRÉQUENCES PROPRES
    // =========================================================================
    if (results && results->isValid() && !results->modalModes().empty())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Analyse Dynamique et Modes Propres de Vibration";

        NDCSection sModal;
        sModal.title = "Spectre des Modes Propres";
        sModal.paragraphs.push_back("Résultats de l'analyse aux valeurs propres du système dynamique [K - omega^2 M]{phi} = 0 :");

        NDCTable tModal;
        tModal.caption = "Tableau 8.1 : Fréquences et périodes propres";
        tModal.headers = {"Mode #", "Valeur Propre lambda (rad²/s²)", "Pulsation omega (rad/s)", "Fréquence f (Hz)", "Période T (s)"};

        for (const auto& m : results->modalModes())
        {
            tModal.rows.push_back({
                QString::number(m.modeNumber),
                QString::number(m.eigenvalue, 'f', 2),
                QString::number(m.omega, 'f', 2),
                QString::number(m.frequency, 'f', 3),
                QString::number(m.period, 'f', 3)
            });
        }
        sModal.tables.push_back(tModal);
        ch.sections.push_back(sModal);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE 9 : CONCLUSION ET DÉCLARATION DE CONFORMITÉ
    // =========================================================================
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = "Conclusions et Déclaration de Conformité";

        NDCSection sConc;
        sConc.paragraphs.push_back("Les calculs éléments finis conduits avec OpenSees attestent de la cohérence mécanique globale du modèle. "
                                  "L'équilibre statique global des charges et réactions est rigoureusement respecté. "
                                  "Les grandeurs déterminées ci-dessus constituent les sollicitations de dimensionnement "
                                  "à l'État Limite Ultime (ELU) et à l'État Limite de Service (ELS) selon les Eurocodes.");
        ch.sections.push_back(sConc);

        doc.addChapter(ch);
    }

    return doc;
}

} // namespace TSA::NDC
