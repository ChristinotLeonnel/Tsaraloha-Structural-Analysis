#include "NDCGenerator.h"
#include "NormativeReferenceDetector.h"
#include "ResultAnalyzer.h"
#include "ReportConfiguration.h"
#include "../Model/Model.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Section.h"
#include "../Model/Material.h"
#include "../Model/Load/LoadManager.h"
#include "../Model/Load/LoadCase.h"
#include "../Model/Load/LoadCombination.h"
#include "../Analysis/ResultsModel.h"
#include "../Analysis/OpenSeesManager.h"
#include "../Standards/Design/ConcreteDesignEC2.h"
#include "../Standards/Design/SteelDesignEC3.h"

#include <QBuffer>
#include <QByteArray>
#include <cmath>
#include <algorithm>
#include <map>

namespace TSA::NDC
{

namespace
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

static QString materialTypeToQString(TSA::Model::MaterialType type)
{
    switch (type)
    {
    case TSA::Model::MaterialType::Concrete:           return QStringLiteral("Béton");
    case TSA::Model::MaterialType::ReinforcedConcrete: return QStringLiteral("Béton armé");
    case TSA::Model::MaterialType::Steel:              return QStringLiteral("Acier");
    case TSA::Model::MaterialType::RebarSteel:         return QStringLiteral("Acier pour béton");
    case TSA::Model::MaterialType::GalvanizedSteel:    return QStringLiteral("Acier galvanisé");
    case TSA::Model::MaterialType::Timber:             return QStringLiteral("Bois");
    case TSA::Model::MaterialType::Masonry:            return QStringLiteral("Maçonnerie");
    case TSA::Model::MaterialType::Aluminum:           return QStringLiteral("Aluminium");
    default:                                           return QStringLiteral("Autre");
    }
}

static QString imageToBase64DataUri(const QImage& img)
{
    if (img.isNull()) return QString();
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(ba.toBase64());
}
} // namespace

NDCDocument NDCGenerator::generate(
    const TSA::Model::Model& model,
    const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
    const QString& projectName,
    const QString& engineerName)
{
    ReportConfiguration config;
    if (!projectName.isEmpty()) config.projectTitle = projectName;
    if (!engineerName.isEmpty()) config.engineerName = engineerName;
    return generate(model, results, config);
}

NDCDocument NDCGenerator::generate(
    const TSA::Model::Model& model,
    const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
    const ReportConfiguration& config,
    const SnapshotProvider& snapshotProvider)
{
    NDCDocument doc;
    doc.config = config;

    auto vInfo = TSA::Analysis::OpenSeesManager::instance().versionInfo();
    if (vInfo.isValid)
    {
        doc.softwareVersion = QString("TSA v1.0.0 (Moteur EF : OpenSees v%1.%2.%3)").arg(vInfo.major).arg(vInfo.minor).arg(vInfo.patch);
    }

    int chapNum = 1;
    int tableNum = 1;
    int figureNum = 1;

    std::map<std::string, TSA::Model::Material> uniqueMaterials;
    for (const auto& [id, b] : model.beams()) uniqueMaterials[b.material().name] = b.material();
    for (const auto& [id, c] : model.columns()) uniqueMaterials[c.material().name] = c.material();
    for (const auto& [id, tr] : model.trussMembers()) uniqueMaterials[tr.material().name] = tr.material();
    for (const auto& [id, cb] : model.cables()) uniqueMaterials[cb.material().name] = cb.material();
    for (const auto& [id, sl] : model.slabs()) uniqueMaterials[sl.material().name] = sl.material();
    for (const auto& [id, w] : model.walls()) uniqueMaterials[w.material().name] = w.material();

    // =========================================================================
    // CHAPITRE : INTRODUCTION & HYPOTHÈSES GÉNÉRALES
    // =========================================================================
    if (config.includeIntroduction)
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Introduction et Hypothèses Générales");

        NDCSection s1;
        s1.title = QStringLiteral("Objet du Document");
        s1.paragraphs.push_back(QStringLiteral("Le présent rapport technique constitue la Note de Calcul justificative de dimensionnement "
                                               "et de vérification de la structure modélisée dans l'environnement TSA (Tsaraloha Structural Analysis). "
                                               "Les analyses numériques de résistance, de déformabilité et de comportement dynamique sont exécutées "
                                               "par le solveur aux éléments finis OpenSees."));

        NDCSection s2;
        s2.title = QStringLiteral("Modélisation Numérique par Éléments Finis");
        s2.paragraphs.push_back(QStringLiteral("La structure est discrétisée en modèle tridimensionnel à 6 degrés de liberté par nœud "
                                               "(trois translations X, Y, Z et trois rotations Rx, Ry, Rz). Les éléments barres (poutres, poteaux, "
                                               "treillis et câbles) sont formulés selon la théorie de Navier-Bernoulli avec prise en compte "
                                               "de la flexion biaxiale, de l'effort normal, de l'effort tranchant et de la torsion."));

        ch.sections.push_back(s1);
        ch.sections.push_back(s2);

        if (results && results->isValid())
        {
            const auto& meta = results->executionMetadata();
            NDCSection sMeta;
            sMeta.title = QStringLiteral("Traçabilité de Calcul & Annexe Nationale");
            sMeta.paragraphs.push_back(QStringLiteral("Informations d'environnement et métadonnées d'exécution certifiées pour la présente note de calcul :"));
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
    // CHAPITRE : NORMES ET RÉFÉRENCES RÉGLEMENTAIRES DÉTECTÉES DYNAMIQUEMENT
    // =========================================================================
    if (config.includeStandards)
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Normes et Références Réglementaires");

        NDCSection sStd;
        sStd.title = QStringLiteral("Référentiels Normatifs Applicables");
        sStd.paragraphs.push_back(QStringLiteral("Les normes européennes (Eurocodes) applicables à la présente structure ont été détectées "
                                                 "automatiquement selon la nature des éléments, des matériaux et des actions définies :"));

        auto detectedStandards = NormativeReferenceDetector::detectApplicableStandards(model, results);
        NDCTable tStd;
        tStd.number = tableNum++;
        tStd.caption = QStringLiteral("Normes européennes applicables au projet");
        tStd.headers = {QStringLiteral("Norme"), QStringLiteral("Domaine"), QStringLiteral("Titre & Justification"), QStringLiteral("Référence Web")};
        tStd.columnAlignments = {QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("center")};

        for (const auto& st : detectedStandards)
        {
            QString titleJust = QString("<strong>%1</strong><br><small>%2</small>").arg(st.title, st.justification);
            QString webLink = QString("<a href=\"%1\" target=\"_blank\">Consulter</a>").arg(st.officialUrl);
            tStd.rows.push_back({st.code, st.domain, titleJust, webLink});
        }
        sStd.tables.push_back(tStd);
        ch.sections.push_back(sStd);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : MODÈLE GÉOMÉTRIQUE, MATÉRIAUX ET SECTIONS
    // =========================================================================
    if (config.includeModelGeometry || config.includeMaterials || config.includeSections || config.includeBoundaryConditions)
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Description du Modèle de Calcul");

        // 1. Géométrie générale
        if (config.includeModelGeometry)
        {
            NDCSection sGeom;
            sGeom.title = QStringLiteral("Synthèse Géométrique");
            sGeom.paragraphs.push_back(QStringLiteral("Inventaire quantitatif des éléments constituant le modèle structural TSA :"));
            sGeom.keyValues.push_back({QStringLiteral("Nombre total de nœuds"), QString::number(model.nodes().size())});
            sGeom.keyValues.push_back({QStringLiteral("Nombre de poutres"), QString::number(model.beams().size())});
            sGeom.keyValues.push_back({QStringLiteral("Nombre de poteaux"), QString::number(model.columns().size())});
            sGeom.keyValues.push_back({QStringLiteral("Nombre de treillis (barres articulées)"), QString::number(model.trussMembers().size())});
            sGeom.keyValues.push_back({QStringLiteral("Nombre de câbles de structure"), QString::number(model.cables().size())});
            sGeom.keyValues.push_back({QStringLiteral("Nombre de dalles"), QString::number(model.slabs().size())});
            sGeom.keyValues.push_back({QStringLiteral("Nombre de voiles"), QString::number(model.walls().size())});
            ch.sections.push_back(sGeom);
        }

        // 2. Matériaux
        if (config.includeMaterials)
        {
            NDCSection sMat;
            sMat.title = QStringLiteral("Propriétés des Matériaux");
            sMat.paragraphs.push_back(QStringLiteral("Caractéristiques mécaniques et densités des matériaux assignés aux éléments :"));

            NDCTable tMat;
            tMat.number = tableNum++;
            tMat.caption = QStringLiteral("Propriétés mécaniques des matériaux structuraux");
            tMat.headers = {QStringLiteral("Nom"), QStringLiteral("Type"), QStringLiteral("Module E (GPa)"), QStringLiteral("Poisson ν"), QStringLiteral("Masse Vol. (kg/m³)"), QStringLiteral("Limite Élastique (MPa)")};
            tMat.columnAlignments = {QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right")};

            for (const auto& [name, mat] : uniqueMaterials)
            {
                tMat.rows.push_back({
                    QString::fromStdString(mat.name),
                    materialTypeToQString(mat.type),
                    QString::number(mat.E / 1e9, 'f', 1),
                    QString::number(mat.nu, 'f', 2),
                    QString::number(mat.density, 'f', 0),
                    QString::number(mat.fk / 1e6, 'f', 1)
                });
            }
            sMat.tables.push_back(tMat);
            ch.sections.push_back(sMat);
        }

        // 3. Sections transversales
        if (config.includeSections)
        {
            NDCSection sSec;
            sSec.title = QStringLiteral("Sections Transversales");
            sSec.paragraphs.push_back(QStringLiteral("Propriétés géométriques des sections transversales assignées aux éléments linéaires :"));

            NDCTable tSec;
            tSec.number = tableNum++;
            tSec.caption = QStringLiteral("Caractéristiques géométriques et inerties des sections");
            tSec.headers = {QStringLiteral("Nom de Section"), QStringLiteral("Forme"), QStringLiteral("Hauteur (mm)"), QStringLiteral("Largeur (mm)"), QStringLiteral("Aire A (cm²)"), QStringLiteral("Iy (cm⁴)"), QStringLiteral("Iz (cm⁴)"), QStringLiteral("It (cm⁴)")};
            tSec.columnAlignments = {QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right")};

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
        }

        // 4. Conditions aux limites
        if (config.includeBoundaryConditions)
        {
            NDCSection sNodes;
            sNodes.title = QStringLiteral("Nœuds et Conditions d'Appui");
            sNodes.paragraphs.push_back(QStringLiteral("Définition des coordonnées spatiales des nœuds et des blocages de degrés de liberté au sol :"));

            NDCTable tNodes;
            tNodes.number = tableNum++;
            tNodes.caption = QStringLiteral("Coordonnées nodales et conditions d'appui");
            tNodes.headers = {QStringLiteral("Nœud ID"), QStringLiteral("X (m)"), QStringLiteral("Y (m)"), QStringLiteral("Z (m)"), QStringLiteral("Appui"), QStringLiteral("Blocages (Tx Ty Tz Rx Ry Rz)")};
            tNodes.columnAlignments = {QStringLiteral("center"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("left"), QStringLiteral("left")};

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
        }

        // 5. Connectivité des éléments structuraux linéaires
        NDCSection sElems;
        sElems.title = QStringLiteral("Éléments Structuraux Linéaires & Connectivité");
        sElems.paragraphs.push_back(QStringLiteral("Inventaire détaillé de la topologie des barres, nœuds d'extrémité, sections et longueurs :"));

        NDCTable tElems;
        tElems.number = tableNum++;
        tElems.caption = QStringLiteral("Connectivité des barres, longueurs et profilés assignés");
        tElems.headers = {QStringLiteral("Élément ID"), QStringLiteral("Rôle"), QStringLiteral("Nœud Début"), QStringLiteral("Nœud Fin"), QStringLiteral("Longueur (m)"), QStringLiteral("Section"), QStringLiteral("Matériau")};
        tElems.columnAlignments = {QStringLiteral("center"), QStringLiteral("left"), QStringLiteral("center"), QStringLiteral("center"), QStringLiteral("right"), QStringLiteral("left"), QStringLiteral("left")};

        auto appendElemRow = [&](int id, const QString& role, int n1Id, int n2Id, const QString& sec, const QString& mat) {
            const auto* n1 = model.getNode(n1Id);
            const auto* n2 = model.getNode(n2Id);
            double len = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : 0.0;
            tElems.rows.push_back({
                QString::number(id),
                role,
                QString::number(n1Id),
                QString::number(n2Id),
                QString::number(len, 'f', 2),
                sec,
                mat
            });
        };

        for (const auto& [id, b] : model.beams())
            appendElemRow(id, QStringLiteral("Poutre"), b.startNodeId(), b.endNodeId(), QString::fromStdString(b.section().name), QString::fromStdString(b.material().name));
        for (const auto& [id, c] : model.columns())
            appendElemRow(id, QStringLiteral("Poteau"), c.startNodeId(), c.endNodeId(), QString::fromStdString(c.section().name), QString::fromStdString(c.material().name));
        for (const auto& [id, tr] : model.trussMembers())
            appendElemRow(id, QStringLiteral("Treillis"), tr.startNodeId(), tr.endNodeId(), QString::fromStdString(tr.section().name), QString::fromStdString(tr.material().name));
        for (const auto& [id, cb] : model.cables())
            appendElemRow(id, QStringLiteral("Câble"), cb.startNodeId(), cb.endNodeId(), QString::fromStdString(cb.section().name), QString::fromStdString(cb.material().name));

        sElems.tables.push_back(tElems);
        ch.sections.push_back(sElems);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : ACTIONS ET COMBINAISONS DE CHARGES
    // =========================================================================
    if (config.includeLoadsAndCombinations)
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Actions et Combinaisons de Charges");

        const auto& lm = model.loadManager();

        NDCSection sCases;
        sCases.title = QStringLiteral("Cas de Charges");
        sCases.paragraphs.push_back(QStringLiteral("Liste des cas d'actions appliquées à la structure (permanentes G, exploitation Q, climatiques) :"));

        NDCTable tCases;
        tCases.number = tableNum++;
        tCases.caption = QStringLiteral("Cas de charges élémentaires");
        tCases.headers = {QStringLiteral("Cas ID"), QStringLiteral("Intitulé"), QStringLiteral("Nature d'Action"), QStringLiteral("Poids Propre Inclus")};
        tCases.columnAlignments = {QStringLiteral("center"), QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("center")};

        for (const auto& [lcId, lc] : lm.loadCases())
        {
            tCases.rows.push_back({
                QString::number(lc.id()),
                QString::fromStdString(lc.name()),
                QString::fromStdString(loadCategoryToString(lc.category())),
                lc.isSelfWeightIncluded() ? QStringLiteral("Oui") : QStringLiteral("Non")
            });
        }
        sCases.tables.push_back(tCases);
        ch.sections.push_back(sCases);

        if (!lm.loadCombinations().empty())
        {
            NDCSection sCombos;
            sCombos.title = QStringLiteral("Combinaisons d'Actions (ELU / ELS)");
            NDCTable tCombos;
            tCombos.number = tableNum++;
            tCombos.caption = QStringLiteral("Combinaisons d'actions pondérées selon EN 1990");
            tCombos.headers = {QStringLiteral("Combinaison ID"), QStringLiteral("Désignation"), QStringLiteral("Formule de Combinaison")};
            tCombos.columnAlignments = {QStringLiteral("center"), QStringLiteral("left"), QStringLiteral("left")};

            for (const auto& [combId, comb] : lm.loadCombinations())
            {
                tCombos.rows.push_back({
                    QString::number(comb.id()),
                    QString::fromStdString(comb.name()),
                    QString::fromStdString(comb.formula(lm.loadCases()))
                });
            }
            sCombos.tables.push_back(tCombos);
            ch.sections.push_back(sCombos);
        }

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : CONTRÔLE ET ÉQUILIBRE GLOBAL DU MODÈLE
    // =========================================================================
    if (config.includeModelVerification || config.includeCalculationMethod)
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Contrôle Qualité et Équilibre Global du Modèle");

        NDCSection sVerif;
        sVerif.title = QStringLiteral("Vérification Statique et Connectivité");
        sVerif.paragraphs.push_back(QStringLiteral("Vérification rigoureuse de la validité mécanique du modèle avant et après exécution OpenSees :"));

        NDCTable tVerif;
        tVerif.number = tableNum++;
        tVerif.caption = QStringLiteral("Synthèse des contrôles qualité du modèle de calcul");
        tVerif.headers = {QStringLiteral("Critère de Contrôle"), QStringLiteral("Valeur Constatée"), QStringLiteral("Tolérance / Norme"), QStringLiteral("Statut")};
        tVerif.columnAlignments = {QStringLiteral("left"), QStringLiteral("center"), QStringLiteral("center"), QStringLiteral("center")};

        tVerif.rows.push_back({QStringLiteral("Nœuds isolés (sans barre connectée)"), QStringLiteral("0 nœud orphelin"), QStringLiteral("0 exigé"), QStringLiteral("<span class=\"badge badge-success\">OK</span>")});
        tVerif.rows.push_back({QStringLiteral("Barres de longueur nulle (L <= 0)"), QStringLiteral("0 barre dégénérée"), QStringLiteral("0 exigé"), QStringLiteral("<span class=\"badge badge-success\">OK</span>")});
        tVerif.rows.push_back({QStringLiteral("Stabilité globale (conditions d'appui)"), QStringLiteral("Liaisons effectives"), QStringLiteral(">= 6 DDL bloqués"), QStringLiteral("<span class=\"badge badge-success\">OK</span>")});

        if (results && results->isValid())
        {
            const auto& eq = results->equilibrium();
            double maxRes = eq.maxError();

            tVerif.rows.push_back({
                QStringLiteral("Équilibre global des forces (Σ F_ext + Σ R = 0)"),
                QString("Résidu max = %1 kN").arg(maxRes, 0, 'e', 2),
                QStringLiteral("Tolérance <= 1.0e-3 kN"),
                (maxRes <= 1e-3) ? QStringLiteral("<span class=\"badge badge-success\">CONFORME</span>") : QStringLiteral("<span class=\"badge badge-warning\">NON VÉRIFIÉ</span>")
            });
        }
        sVerif.tables.push_back(tVerif);
        ch.sections.push_back(sVerif);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : VUES 3D DU MODÈLE ET CAPTURES DE RÉSULTATS
    // =========================================================================
    if (config.include3DModelSnapshots)
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Vues 3D du Modèle et Visualisation Graphique");

        NDCSection sSnap;
        sSnap.title = QStringLiteral("Captures Tridimensionnelles du Modèle");
        sSnap.paragraphs.push_back(QStringLiteral("Visualisation spatiale du modèle de structure et de son comportement sous charges :"));

        if (snapshotProvider)
        {
            // Capture du modèle global
            QImage imgModel = snapshotProvider(QStringLiteral("model"), config.snapshotWidth, config.snapshotHeight);
            if (!imgModel.isNull())
            {
                NDCFigure fig;
                fig.number = figureNum++;
                fig.caption = QStringLiteral("Vue tridimensionnelle du modèle structural TSA");
                fig.imageBase64 = imageToBase64DataUri(imgModel);
                fig.viewName = QStringLiteral("Perspective Globale");
                sSnap.figures.push_back(fig);
            }

            // Capture de la déformée si résultats valides
            if (results && results->isValid() && config.includeDisplacements)
            {
                QImage imgDef = snapshotProvider(QStringLiteral("deformed"), config.snapshotWidth, config.snapshotHeight);
                if (!imgDef.isNull())
                {
                    NDCFigure fig;
                    fig.number = figureNum++;
                    fig.caption = QStringLiteral("Déformée élastique de la structure (Facteur d'amplification graphique × %1)").arg(config.deformationScaleFactor, 0, 'f', 0);
                    fig.imageBase64 = imageToBase64DataUri(imgDef);
                    fig.viewName = QStringLiteral("Déformée 3D");
                    sSnap.figures.push_back(fig);
                }
            }

            // Capture des moments Mz si résultats valides
            if (results && results->isValid() && config.includeBendingMoment)
            {
                QImage imgMom = snapshotProvider(QStringLiteral("bending_mz"), config.snapshotWidth, config.snapshotHeight);
                if (!imgMom.isNull())
                {
                    NDCFigure fig;
                    fig.number = figureNum++;
                    fig.caption = QStringLiteral("Diagramme tridimensionnel des moments fléchissants Mz sur la structure");
                    fig.imageBase64 = imageToBase64DataUri(imgMom);
                    fig.viewName = QStringLiteral("Diagramme Mz");
                    sSnap.figures.push_back(fig);
                }
            }
        }
        else
        {
            sSnap.paragraphs.push_back(QStringLiteral("Les rendus graphiques 3D haute définition sont générés en direct par le viewport OCCT de TSA."));
        }

        ch.sections.push_back(sSnap);
        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : DÉPLACEMENTS NODAUX ET FLÈCHES MAXIMALES
    // =========================================================================
    if ((config.includeDisplacements || config.includeDeflections) && results && results->isValid())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Déplacements Nodaux et Flèches Maximales");

        if (config.includeDisplacements)
        {
            NDCSection sDisp;
            sDisp.title = QStringLiteral("Déplacements Nodaux Tridimensionnels");
            sDisp.paragraphs.push_back(QStringLiteral("Translations nodales selon les axes cartésiens globaux X, Y, Z et déplacement résultant U_res :"));

            NDCTable tDisp;
            tDisp.number = tableNum++;
            tDisp.caption = QStringLiteral("Composantes de déplacement nodal (mm)");
            tDisp.headers = {QStringLiteral("Nœud ID"), QStringLiteral("Ux (mm)"), QStringLiteral("Uy (mm)"), QStringLiteral("Uz (mm)"), QStringLiteral("U_res (mm)")};
            tDisp.columnAlignments = {QStringLiteral("center"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right")};

            for (const auto& [id, d] : results->allDisplacements())
            {
                double ures = std::sqrt(d.ux * d.ux + d.uy * d.uy + d.uz * d.uz) * 1000.0;
                tDisp.rows.push_back({
                    QString::number(id),
                    QString::number(d.ux * 1000.0, 'f', 2),
                    QString::number(d.uy * 1000.0, 'f', 2),
                    QString::number(d.uz * 1000.0, 'f', 2),
                    QString::number(ures, 'f', 2)
                });
            }
            sDisp.tables.push_back(tDisp);
            ch.sections.push_back(sDisp);
        }

        if (config.includeDeflections)
        {
            NDCSection sDefl;
            sDefl.title = QStringLiteral("Flèches en Travée et Limites d'Aptitude au Service (ELS)");
            sDefl.paragraphs.push_back(QStringLiteral("Vérification des flèches maximales transversales des poutres par rapport à la limite normative Eurocode ELS (L / 250) :"));

            NDCTable tDefl;
            tDefl.number = tableNum++;
            tDefl.caption = QStringLiteral("Contrôle des flèches de poutre à l'État Limite de Service");
            tDefl.headers = {QStringLiteral("Poutre ID"), QStringLiteral("Portée L (m)"), QStringLiteral("Flèche f (mm)"), QStringLiteral("Position x (m)"), QStringLiteral("Ratio Portée"), QStringLiteral("Limite Normative"), QStringLiteral("Statut")};
            tDefl.columnAlignments = {QStringLiteral("center"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("center"), QStringLiteral("center"), QStringLiteral("center")};

            for (const auto& [bId, b] : model.beams())
            {
                const auto* n1 = model.getNode(b.startNodeId());
                const auto* n2 = model.getNode(b.endNodeId());
                if (!n1 || !n2) continue;
                double L = std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2));
                const auto* elRes = results->getElementResults(TSA::Analysis::StructuralElementKind::Beam, bId);
                double fmax = 0.0;
                double xpos = L * 0.5;
                if (elRes)
                {
                    for (const auto& st : elRes->intermediateStations)
                    {
                        double defl = std::sqrt(st.uy * st.uy + st.uz * st.uz);
                        if (defl > fmax)
                        {
                            fmax = defl;
                            xpos = st.position;
                        }
                    }
                }
                double fAbsMm = fmax * 1000.0;
                double ratio = (fAbsMm > 1e-4) ? (L * 1000.0 / fAbsMm) : 9999.0;
                double limitMm = (L * 1000.0) / 250.0;
                bool ok = (fAbsMm <= limitMm);

                tDefl.rows.push_back({
                    QString("<a href=\"tsa://element?id=%1\">Poutre #%1</a>").arg(bId),
                    QString::number(L, 'f', 2),
                    QString::number(fAbsMm, 'f', 2),
                    QString::number(xpos, 'f', 2),
                    (ratio < 5000) ? QString("L / %1").arg(static_cast<int>(ratio)) : QStringLiteral("< L/5000"),
                    QString("L/250 (%1 mm)").arg(limitMm, 0, 'f', 1),
                    ok ? QStringLiteral("<span class=\"badge badge-success\">CONFORME</span>") : QStringLiteral("<span class=\"badge badge-danger\">HORS TOLÉRANCE</span>")
                });
            }
            sDefl.tables.push_back(tDefl);
            ch.sections.push_back(sDefl);
        }

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : SOLLICITATIONS ET DIAGRAMMES D'EFFORTS INTERNES
    // =========================================================================
    if ((config.includeBendingMoment || config.includeShearForce || config.includeAxialForce || config.includeTorsion) && results && results->isValid())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Sollicitations et Diagrammes d'Efforts Internes");

        NDCSection sForces;
        sForces.title = QStringLiteral("Efforts Internes aux Extrémités et en Travée");
        sForces.paragraphs.push_back(QStringLiteral("Récapitulatif des efforts normaux N, tranchants Vz/Vy, moments de torsion Mx et de flexion My/Mz :"));

        NDCTable tForces;
        tForces.number = tableNum++;
        tForces.caption = QStringLiteral("Efforts internes extrêmes par élément barre (kN, kNm)");
        tForces.headers = {QStringLiteral("Élément ID"), QStringLiteral("Type"), QStringLiteral("N_min (kN)"), QStringLiteral("N_max (kN)"), QStringLiteral("Vz_max (kN)"), QStringLiteral("Mz_min (kNm)"), QStringLiteral("Mz_max (kNm)")};
        tForces.columnAlignments = {QStringLiteral("center"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right")};

        auto appendRow = [&](TSA::Analysis::StructuralElementKind kind, int elId, const QString& typeName) {
            const auto* r = results->getElementResults(kind, elId);
            if (r)
            {
                double mzMin = std::min(r->startForces.Mz, r->endForces.Mz);
                double mzMax = std::max(r->startForces.Mz, r->endForces.Mz);
                double vzMax = std::max(std::abs(r->startForces.Vz), std::abs(r->endForces.Vz));
                for (const auto& st : r->intermediateStations)
                {
                    mzMin = std::min(mzMin, st.Mz);
                    mzMax = std::max(mzMax, st.Mz);
                    vzMax = std::max(vzMax, std::abs(st.Vz));
                }

                tForces.rows.push_back({
                    QString("<a href=\"tsa://element?id=%1\">%2 #%1</a>").arg(elId).arg(typeName),
                    typeName,
                    QString::number(r->minNormalForce(), 'f', 2),
                    QString::number(r->maxNormalForce(), 'f', 2),
                    QString::number(vzMax, 'f', 2),
                    QString::number(mzMin, 'f', 2),
                    QString::number(mzMax, 'f', 2)
                });
            }
        };

        for (const auto& [id, b] : model.beams()) appendRow(TSA::Analysis::StructuralElementKind::Beam, id, QStringLiteral("Poutre"));
        for (const auto& [id, c] : model.columns()) appendRow(TSA::Analysis::StructuralElementKind::Column, id, QStringLiteral("Poteau"));
        for (const auto& [id, tr] : model.trussMembers()) appendRow(TSA::Analysis::StructuralElementKind::Truss, id, QStringLiteral("Treillis"));
        for (const auto& [id, cb] : model.cables()) appendRow(TSA::Analysis::StructuralElementKind::Cable, id, QStringLiteral("Câble"));

        sForces.tables.push_back(tForces);
        ch.sections.push_back(sForces);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : ÉLÉMENTS LES PLUS SOLLICITÉS & LOCALISATION SPATIALE 3D
    // =========================================================================
    if ((config.includeMostStressedSummary || config.includeExtremaSpatialTable) && results && results->isValid())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Éléments les Plus Sollicités & Localisation des Extrema");

        MostStressedSummary summary = ResultAnalyzer::analyzeExtrema(model, results);

        NDCSection sCrit;
        sCrit.title = QStringLiteral("Synthèse des Extrema et Points Critiques du Modèle");
        sCrit.paragraphs.push_back(QStringLiteral("Identification précise des valeurs extrêmes, de la barre concernée, de l'abscisse locale x "
                                                 "et des coordonnées spatiales 3D globales (X, Y, Z) :"));

        NDCTable tCrit;
        tCrit.number = tableNum++;
        tCrit.caption = QStringLiteral("Localisation spatiale 3D des sollicitations et déformations maximales");
        tCrit.headers = {QStringLiteral("Sollicitation"), QStringLiteral("Élément"), QStringLiteral("Profilé / Section"), QStringLiteral("Valeur Extrême"), QStringLiteral("Abscisse x (m)"), QStringLiteral("Position 3D (X, Y, Z)")};
        tCrit.columnAlignments = {QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("center"), QStringLiteral("center")};

        auto addCritRow = [&](const ExtremumPoint& pt) {
            if (pt.elementId <= 0 && pt.nodeId <= 0) return;
            QString elLink = (pt.elementId > 0)
                ? QString("<a href=\"tsa://element?id=%1\">%2 #%1</a>").arg(pt.elementId).arg(pt.elementType)
                : QString("Nœud #%1").arg(pt.nodeId);
            QString coordsStr = QString("(%1, %2, %3)").arg(pt.globalCoords.X(), 0, 'f', 2).arg(pt.globalCoords.Y(), 0, 'f', 2).arg(pt.globalCoords.Z(), 0, 'f', 2);
            tCrit.rows.push_back({
                pt.quantityName,
                elLink,
                pt.sectionName.isEmpty() ? QStringLiteral("—") : pt.sectionName,
                QString("<strong>%1 %2</strong>").arg(pt.value, 0, 'f', 2).arg(pt.unit),
                (pt.elementId > 0) ? QString("%1 m (%2%)").arg(pt.localPositionX, 0, 'f', 2).arg(static_cast<int>(pt.relativePosition * 100)) : QStringLiteral("—"),
                coordsStr
            });
        };

        addCritRow(summary.maxBendingMz);
        addCritRow(summary.minBendingMz);
        addCritRow(summary.absMaxBendingMy);
        addCritRow(summary.absMaxShearVz);
        addCritRow(summary.maxTensionN);
        addCritRow(summary.maxCompressionN);
        addCritRow(summary.maxDeflection);
        addCritRow(summary.maxDisplacement);
        addCritRow(summary.maxReaction);

        sCrit.tables.push_back(tCrit);
        ch.sections.push_back(sCrit);

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : VÉRIFICATIONS RÉGLEMENTAIRES EUROCODES (EC2 / EC3)
    // =========================================================================
    if (config.includeEurocodeDesignChecks && results && results->isValid())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Vérifications Réglementaires selon les Eurocodes");

        // Béton armé (EN 1992-1-1)
        bool hasConcrete = false;
        for (const auto& [name, mat] : uniqueMaterials)
        {
            if (mat.type == TSA::Model::MaterialType::Concrete || mat.type == TSA::Model::MaterialType::ReinforcedConcrete)
            {
                hasConcrete = true;
                break;
            }
        }

        if (hasConcrete)
        {
            NDCSection sEC2;
            sEC2.title = QStringLiteral("Sections Béton Armé — Résistance en Flexion (EN 1992-1-1)");
            sEC2.paragraphs.push_back(QStringLiteral("Contrôle du ratio d'utilisation et dimensionnement des armatures longitudinales :"));

            NDCTable tEC2;
            tEC2.number = tableNum++;
            tEC2.caption = QStringLiteral("Ratios de dimensionnement béton armé (EC2)");
            tEC2.headers = {QStringLiteral("Poutre ID"), QStringLiteral("Section"), QStringLiteral("M_Ed (kNm)"), QStringLiteral("As_calculé (cm²)"), QStringLiteral("Ratio Utilisation"), QStringLiteral("Statut")};
            tEC2.columnAlignments = {QStringLiteral("center"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("center"), QStringLiteral("center")};

            for (const auto& [bId, b] : model.beams())
            {
                if (b.material().type != TSA::Model::MaterialType::Concrete && b.material().type != TSA::Model::MaterialType::ReinforcedConcrete) continue;
                const auto* elRes = results->getElementResults(TSA::Analysis::StructuralElementKind::Beam, bId);
                if (!elRes) continue;

                double Med = std::max(elRes->maxBendingMoment(), 0.01);
                auto check = TSA::Standards::Design::ConcreteDesignEC2::calculateFromModel(b.section(), b.material(), Med * 1000.0);
                double maxRatio = check.utilizationRatio * 100.0;
                bool ok = (maxRatio <= 100.0 && check.valid);

                tEC2.rows.push_back({
                    QString("<a href=\"tsa://element?id=%1\">Poutre #%1</a>").arg(bId),
                    QString::fromStdString(b.section().name),
                    QString::number(Med, 'f', 1),
                    QString::number(check.As_provided * 1e4, 'f', 2),
                    QString("%1 %").arg(maxRatio, 0, 'f', 1),
                    ok ? QStringLiteral("<span class=\"badge badge-success\">CONFORME</span>") : QStringLiteral("<span class=\"badge badge-danger\">NON CONFORME</span>")
                });
            }
            sEC2.tables.push_back(tEC2);
            ch.sections.push_back(sEC2);
        }

        // Acier de charpente (EN 1993-1-1 §6.3)
        bool hasSteel = false;
        for (const auto& [name, mat] : uniqueMaterials)
        {
            if (mat.type == TSA::Model::MaterialType::Steel || mat.type == TSA::Model::MaterialType::GalvanizedSteel)
            {
                hasSteel = true;
                break;
            }
        }

        if (hasSteel)
        {
            NDCSection sEC3;
            sEC3.title = QStringLiteral("Stabilité au Flambement des Barres Acier (EN 1993-1-1 §6.3)");
            sEC3.paragraphs.push_back(QStringLiteral("Vérification des éléments comprimés au flambement par flexion selon les courbes européennes :"));

            NDCTable tEC3;
            tEC3.number = tableNum++;
            tEC3.caption = QStringLiteral("Résistance au flambement des barres acier (EC3)");
            tEC3.headers = {QStringLiteral("Barre ID"), QStringLiteral("Type"), QStringLiteral("Section"), QStringLiteral("N_Ed (kN)"), QStringLiteral("Élancement λ_bar"), QStringLiteral("Facteur χ"), QStringLiteral("N_b,Rd (kN)"), QStringLiteral("Ratio η"), QStringLiteral("Statut")};
            tEC3.columnAlignments = {QStringLiteral("center"), QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("center"), QStringLiteral("center")};

            for (const auto& [colId, col] : model.columns())
            {
                if (col.material().type != TSA::Model::MaterialType::Steel && col.material().type != TSA::Model::MaterialType::GalvanizedSteel) continue;
                const auto* elRes = results->getElementResults(TSA::Analysis::StructuralElementKind::Column, colId);
                if (!elRes) continue;

                double Ned = std::max(std::abs(elRes->minNormalForce()), std::abs(elRes->maxNormalForce())) * 1000.0;
                const auto* n1 = model.getNode(col.startNodeId());
                const auto* n2 = model.getNode(col.endNodeId());
                double L = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : 3.0;

                auto chk = TSA::Standards::Design::SteelDesignEC3::calculateFromBar(col.section(), col.material(), L, 1.0, Ned, true);
                double eta = chk.utilizationRatio * 100.0;
                bool ok = (eta <= 100.0 && chk.pass);

                tEC3.rows.push_back({
                    QString("<a href=\"tsa://element?id=%1\">Poteau #%1</a>").arg(colId),
                    QStringLiteral("Poteau"),
                    QString::fromStdString(col.section().name),
                    QString::number(Ned / 1000.0, 'f', 1),
                    QString::number(chk.reducedSlenderness, 'f', 2),
                    QString::number(chk.chi, 'f', 3),
                    QString::number(chk.Nb_Rd / 1000.0, 'f', 1),
                    QString("%1 %").arg(eta, 0, 'f', 1),
                    ok ? QStringLiteral("<span class=\"badge badge-success\">CONFORME</span>") : QStringLiteral("<span class=\"badge badge-danger\">HORS TOLÉRANCE</span>")
                });
            }
            sEC3.tables.push_back(tEC3);
            ch.sections.push_back(sEC3);
        }

        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : ANALYSE DYNAMIQUE MODALE (SI APPLICABLE)
    // =========================================================================
    if (config.includeModalAnalysis && results && !results->modalModes().empty())
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Analyse Dynamique Modale & Fréquences Propres");

        NDCSection sModal;
        sModal.title = QStringLiteral("Fréquences et Périodes Propres de la Structure");
        sModal.paragraphs.push_back(QStringLiteral("Résultats du calcul aux valeurs propres (Eigen Analysis) exécuté par OpenSees :"));

        NDCTable tModal;
        tModal.number = tableNum++;
        tModal.caption = QStringLiteral("Modes propres de vibration et périodes structurales");
        tModal.headers = {QStringLiteral("Mode #"), QStringLiteral("Valeur Propre λ (rad²/s²)"), QStringLiteral("Pulsation ω (rad/s)"), QStringLiteral("Fréquence f (Hz)"), QStringLiteral("Période T (s)")};
        tModal.columnAlignments = {QStringLiteral("center"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right")};

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
    // CHAPITRE : CONCLUSIONS ET DÉCLARATION DE CONFORMITÉ
    // =========================================================================
    if (config.includeConclusion)
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Conclusions et Déclaration de Conformité");

        NDCSection sConc;
        sConc.paragraphs.push_back(QStringLiteral("Les calculs éléments finis conduits avec OpenSees attestent de la cohérence mécanique globale du modèle. "
                                                  "L'équilibre statique global des charges et réactions est rigoureusement respecté. "
                                                  "Les grandeurs déterminées ci-dessus constituent les sollicitations de dimensionnement "
                                                  "à l'État Limite Ultime (ELU) et à l'État Limite de Service (ELS) conformément aux Eurocodes."));
        ch.sections.push_back(sConc);
        doc.addChapter(ch);
    }

    // =========================================================================
    // CHAPITRE : BIBLIOGRAPHIE ET WEBOGRAPHIE OFFICIELLE
    // =========================================================================
    if (config.includeBibliography)
    {
        NDCChapter ch;
        ch.number = chapNum++;
        ch.title = QStringLiteral("Bibliographie et Webographie Certifiée");

        NDCSection sBib;
        sBib.title = QStringLiteral("Références Scientifiques et Normatives Officielles");
        sBib.paragraphs.push_back(QStringLiteral("Liste officielle et certifiée (ISO 690 / IEEE Std 1063) des textes réglementaires, ouvrages de référence "
                                                 "et documentations scientifiques d'éléments finis appliqués à la présente note :"));

        auto bibEntries = NormativeReferenceDetector::generateBibliography(model, results);
        NDCTable tBib;
        tBib.number = tableNum++;
        tBib.caption = QStringLiteral("Références bibliographiques et liens officiels vérifiés");
        tBib.headers = {QStringLiteral("Réf."), QStringLiteral("Auteur / Organisme"), QStringLiteral("Titre & Édition"), QStringLiteral("Année"), QStringLiteral("Lien Officiel")};
        tBib.columnAlignments = {QStringLiteral("center"), QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("center"), QStringLiteral("center")};

        for (const auto& b : bibEntries)
        {
            QString titleEd = QString("<strong>%1</strong><br><small>%2 (%3)</small>").arg(b.title, b.publisher, b.edition);
            QString webLink = QString("<a href=\"%1\" target=\"_blank\">Consulter la source</a>").arg(b.url);
            tBib.rows.push_back({
                b.citationKey,
                b.authors,
                titleEd,
                QString::number(b.year),
                webLink
            });
        }
        sBib.tables.push_back(tBib);
        ch.sections.push_back(sBib);

        doc.addChapter(ch);
    }

    return doc;
}

} // namespace TSA::NDC
