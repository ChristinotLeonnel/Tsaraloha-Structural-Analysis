// Suite « display » : affichage des résultats, des annotations et des charges dans la vue 3D réelle (OCCT).
//   371  résultats résiduels : portique dont les ids de poutre et de poteau coïncident ; diagrammes, déformée et
//        réactions affichés puis retirés (nouveau jeu de résultats, absence de résultats, modèle vidé = nouveau
//        projet) : aucun objet orphelin dans la scène, aucun cumul après plusieurs calculs ;
//   372  échelles distinctes : zoom de la caméra, échelle des diagrammes (multiplicateur conservé), amplification de la
//        déformée ; aucune ne modifie les résultats calculés ; diagrammes de faible amplitude lisibles (flèches,
//        rotations) ; grandeur nulle partout annoncée sans diagramme arbitraire ;
//   373  annotations : taille constante à l'écran (pixels × DPI) ou liée au modèle (mètres, texte zoomable), bornes
//        8–32 px, facteurs de rôle, réinitialisation, adaptation automatique ; la vue reconstruit ses textes sans
//        objet orphelin ; le zoom ne reconstruit rien ;
//   374  échelle des charges : familles d'unités séparées, modes proportionnel / automatique / normalisé, rapports
//        d'intensité (10, 50, 100 kN), longueurs bornées, valeurs des charges inchangées, nouvelle charge maximale
//        appliquée à toute la famille, légende ;
//   375  copier-coller entre projets : presse-papier commun, projet source détruit avant le collage ; géométrie,
//        sections, matériaux, orientation, appuis, voile, treillis, dalle complète ; charges en option avec cas de
//        charge rapprochés par nom (créés s'ils manquent), valeurs identiques ; collage à l'origine ou au point
//        désigné ; une seule étape d'annulation ; aucun collage partiel (dépendance manquante → modèle inchangé) ;
//   376  diagnostic d'affichage : nombre d'objets de la scène et de résultats conforme à la vue, échelles graphiques
//        listées (caméra, déformée, diagrammes, annotations, charges par famille) ; sa lecture ne modifie rien ;
//   531  courbes 2D superposées (dock « Diagrammes 2D ») : plusieurs grandeurs à la fois (N/Vx, Vy, Vz), ordre stable,
//        dernière courbe conservée, ruban → une seule courbe ; interpolation (saut sous charge ponctuelle), extrêmes
//        à leur vraie position, pas de graduation ; un axe par unité (deux au plus, sinon normalisée) ; poutre et
//        poteau de même id distingués ; tableau de valeurs ; chaque courbe dessinée (superposées et empilées).

#include <QDir>
#include "Model/SelectionQuery.h"
#include <TopExp_Explorer.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <AIS_TextLabel.hxx>
#include "test_common.h"

#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Grid/GridManager.h"
#include "Model/Load/LoadManager.h"
#include "Model/StructuralClipboard.h"
#include "Viewer/OccView.h"
#include "Viewer/ResultsVisualManager.h"
#include "Viewer/AnnotationStyle.h"
#include "Geometry/LoadDisplayScale.h"
#include "Viewer/LoadDisplayStyle.h"
#include "UI/Diagrams/Diagram2DWidget.h"

#include <Prs3d_TextAspect.hxx>

#include <AIS_ListOfInteractive.hxx>
#include <Bnd_Box.hxx>
#include <QApplication>
#include <QImage>
#include <QLocale>
#include <QSettings>
#include <QThread>

#include <set>
#include <vector>

using namespace TSA::Analysis;

namespace
{
int displayedObjects(OccView& v)
{
    AIS_ListOfInteractive list;
    v.context()->DisplayedObjects(list);
    return list.Extent();
}

/// Objets affichés une fois la vue stabilisée : une partie des repères (grilles, plan de travail) est affichée par des
/// minuteries différées ; un compte pris trop tôt les voyait apparaître ensuite (test 371 intermittent, BUG-174).
int settledDisplayedObjects(OccView& v)
{
    int last = displayedObjects(v), stable = 0;
    for (int k = 0; k < 600 && stable < 15; ++k)
    {
        QApplication::processEvents();
        QThread::msleep(5);
        const int now = displayedObjects(v);
        stable = now == last ? stable + 1 : 0;
        last = now;
    }
    return last;
}

void pump(int n = 10)
{
    for (int k = 0; k < n; ++k)
    {
        QApplication::processEvents();
        QThread::msleep(5);
    }
}

/// Portique à deux poteaux (ids 1, 2) et une poutre (id 1) : la poutre 1 et le poteau 1 ont le même id.
std::shared_ptr<ResultsModel> portalResults(Model& m, double h = 3.0, double load = 20.0)
{
    const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, h), c = m.addNode(6, 0, h), d = m.addNode(6, 0, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    m.getNode(d)->setSupport(SupportDefinition::fixed());
    m.addColumn(a, b, Section::heb(200), Material::steelS235());
    const int beam = m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
    m.addColumn(d, c, Section::heb(200), Material::steelS235());
    const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
    m.loadManager().addMemberLoad(MemberLoad::uniform(beam, lc, -load, LoadDirection::GlobalZ));
    m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 10.0, 0.0, 0.0));
    AnalysisEngineRegistry reg;
    registerBuiltInEngines(reg);
    AnalysisManager mgr(reg);
    AnalysisContext ctx;
    ctx.engineId = "opensees";
    ctx.common.includeSelfWeight = false;
    ctx.loadCaseIds = { lc };
    GridManager gm;
    const auto prep = mgr.prepare(m, &gm, ctx);
    if (!prep.canRun()) return nullptr;
    auto run = mgr.run(ctx, prep);
    return run.success ? std::make_shared<ResultsModel>(run.results) : nullptr;
}
} // namespace

bool runSuite_Display(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 531 : courbes 2D superposées (Diagram2DWidget) — en tête : n'exige ni OpenSees ni vue OCCT
    // -------------------------------------------------------------------------
    {
        namespace D2 = TSA::UI::Diagram2D;
        using DT = TSA::Geometry::DiagramType;
        using W = TSA::UI::Diagram2DWidget;

        // Poutre de 5 m : stations 0 ; 1,25 ; 2,5 (deux fois : saut de Vy sous une charge ponctuelle) ; 3,75 ; 5.
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0), c = m.addNode(0, 0, 3);
        const int beam = m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int column = m.addColumn(a, c, Section::heb(200), Material::steelS235());
        TEST_CHECK(beam == column, "Test 531: poutre et poteau de même id (" << beam << ", " << column << ")");

        auto station = [](double x, double n, double vy, double vz, double mz) {
            StationForces st;
            st.position = x; st.N = n; st.Vy = vy; st.Vz = vz; st.Mz = mz; st.uz = -0.002 * x * (5.0 - x);
            return st;
        };
        ElementResults er;
        er.elementId = beam; er.kind = StructuralElementKind::Beam; er.length = 5.0;
        er.startForces = station(0.0, 12.0, 30.0, -4.0, 0.0);
        er.intermediateStations = {station(1.25, 12.0, 15.0, -2.0, 28.0), station(2.5, 12.0, 10.0, 0.0, 45.0),
                                   station(2.5, 12.0, -10.0, 0.0, 45.0), station(3.75, 12.0, -15.0, 2.0, 28.0)};
        er.endForces = station(5.0, 12.0, -30.0, 4.0, 0.0);
        ElementResults ec;
        ec.elementId = column; ec.kind = StructuralElementKind::Column; ec.length = 3.0;
        ec.startForces = station(0.0, -80.0, 5.0, 0.0, 9.0);
        ec.endForces = station(3.0, -80.0, 5.0, 0.0, -6.0);
        auto results = std::make_shared<ResultsModel>();
        results->setElementResults(er);
        results->setElementResults(ec);
        results->setValid(true);

        // Calculs purs : interpolation, saut, extrêmes à leur position, pas « ronds ».
        const auto vy = D2::sampleCurve(er, DT::ShearForceVy, er.length);
        TEST_CHECK(vy.size() == 6 && approxEqual(vy[1].s, 0.25) && approxEqual(vy.back().s, 1.0), "Test 531: stations ramenées à x / L");
        TEST_CHECK(approxEqual(D2::interpolate(vy, 0.125), 22.5) && approxEqual(D2::interpolate(vy, 0.5), 10.0)
                       && approxEqual(D2::interpolate(vy, 0.625), -12.5) && approxEqual(D2::interpolate(vy, -1.0), 30.0)
                       && approxEqual(D2::interpolate(vy, 2.0), -30.0),
                   "Test 531: interpolation linéaire, saut sous charge ponctuelle, bornes de la barre");
        const auto ext = D2::extremes(D2::sampleCurve(er, DT::BendingMz, er.length));
        TEST_CHECK(approxEqual(ext.maxV, 45.0) && approxEqual(ext.maxS, 0.5) && approxEqual(ext.minV, 0.0) && approxEqual(ext.minS, 0.0),
                   "Test 531: extrêmes de Mz et leur position (max à mi-portée)");
        const auto extVz = D2::extremes(D2::sampleCurve(er, DT::ShearForceVz, er.length));
        TEST_CHECK(approxEqual(extVz.maxS, 1.0) && approxEqual(extVz.minS, 0.0), "Test 531: extrêmes de Vz aux extrémités");
        TEST_CHECK(D2::niceStep(10.0, 5) == 2.0 && std::abs(D2::niceStep(0.0007, 4) - 0.0002) < 1e-15 && D2::niceStep(0.0, 4) == 0.0
                       && D2::niceStep(95.0, 10) == 10.0,
                   "Test 531: pas de graduation 1, 2, 5 × 10^n");

        W w;
        w.resize(900, 500);
        w.setModel(&m);
        TEST_CHECK(w.valuesTable().isEmpty(), "Test 531: sans résultat, aucune valeur");
        w.setResultsModel(results);
        w.setSelectedElement(beam, StructuralElementKind::Beam);
        w.setCurveLayout(W::Layout::Overlay);
        w.setScaling(W::Scaling::PerUnit);

        // Plusieurs courbes à la fois, ordre stable, dernière courbe conservée.
        w.setDiagramTypes({DT::ShearForceVz, DT::AxialForceN, DT::ShearForceVy});
        TEST_CHECK((w.diagramTypes() == std::vector<DT>{DT::AxialForceN, DT::ShearForceVy, DT::ShearForceVz}) && w.diagramType() == DT::AxialForceN,
                   "Test 531: N (Vx), Vy et Vz superposés, dans l'ordre des grandeurs");
        w.setDiagramTypeVisible(DT::ShearForceVy, false);
        TEST_CHECK((w.diagramTypes() == std::vector<DT>{DT::AxialForceN, DT::ShearForceVz}), "Test 531: retrait d'une courbe");
        w.setDiagramTypeVisible(DT::AxialForceN, false);
        w.setDiagramTypeVisible(DT::ShearForceVz, false);
        TEST_CHECK((w.diagramTypes() == std::vector<DT>{DT::ShearForceVz}), "Test 531: la dernière courbe n'est jamais retirée");
        w.setDiagramType(DT::DeflectionUz);
        TEST_CHECK((w.diagramTypes() == std::vector<DT>{DT::DeflectionUz}), "Test 531: commande du ruban (flèche uz) = une seule courbe");
        w.setDiagramType(DT::None);
        TEST_CHECK((w.diagramTypes() == std::vector<DT>{DT::DeflectionUz}), "Test 531: grandeur inconnue du widget ignorée");

        // Échelles : un axe par unité (deux au plus), sinon normalisée ; bandes empilées toujours en valeurs réelles.
        w.setDiagramTypes({DT::AxialForceN, DT::ShearForceVy, DT::ShearForceVz});
        TEST_CHECK(w.effectiveScaling() == W::Scaling::PerUnit, "Test 531: N, Vy, Vz : un seul axe (kN)");
        w.setDiagramTypes({DT::ShearForceVy, DT::BendingMz});
        TEST_CHECK(w.effectiveScaling() == W::Scaling::PerUnit, "Test 531: Vy et Mz : deux axes (kN, kNm)");
        w.setDiagramTypes({DT::ShearForceVy, DT::BendingMz, DT::DeflectionUz});
        TEST_CHECK(w.effectiveScaling() == W::Scaling::Normalized, "Test 531: trois unités superposées : échelle normalisée");
        w.setCurveLayout(W::Layout::Stacked);
        TEST_CHECK(w.effectiveScaling() == W::Scaling::PerUnit, "Test 531: bandes empilées : valeurs réelles");
        w.setCurveLayout(W::Layout::Overlay);

        // Tableau des valeurs : une ligne par station, une colonne par courbe.
        w.setDiagramTypes({DT::AxialForceN, DT::ShearForceVy, DT::ShearForceVz});
        const QStringList rows = w.valuesTable().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        const QLocale locale;
        TEST_CHECK(rows.size() == 7 && rows.front() == QStringLiteral("x (m)\tN (Vx) (kN)\tVy (kN)\tVz (kN)"),
                   "Test 531: en-tête du tableau (" << rows.value(0).toStdString() << ")");
        const QStringList r4 = rows[4].split(QLatin1Char('\t'));
        TEST_CHECK(r4.size() == 4 && approxEqual(locale.toDouble(r4[0]), 2.5) && approxEqual(locale.toDouble(r4[1]), 12.0)
                       && approxEqual(locale.toDouble(r4[2]), -10.0) && approxEqual(locale.toDouble(r4[3]), 0.0),
                   "Test 531: valeurs de la station 2,5 m après le saut (" << rows[4].toStdString() << ")");

        // Poteau de même id : ses propres valeurs.
        w.setSelectedElement(column, StructuralElementKind::Column);
        TEST_CHECK(w.selectedElementKind() == StructuralElementKind::Column && w.valuesTable().split(QLatin1Char('\n'), Qt::SkipEmptyParts).size() == 3
                       && w.valuesTable().contains(locale.toString(-80.0, 'g', 8)),
                   "Test 531: poteau et poutre de même id distingués");
        w.setSelectedElement(beam, StructuralElementKind::Beam);

        // Dessin : chaque courbe est tracée (pixels de sa couleur bien au-delà de la légende), superposées et empilées.
        auto colorCount = [](const QImage& img, const QColor& col) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    if (img.pixel(x, y) == col.rgb()) ++n;
            return n;
        };
        for (W::Layout layout : {W::Layout::Overlay, W::Layout::Stacked})
        {
            w.setCurveLayout(layout);
            const QImage img = w.renderImage(QSize(900, 450)).convertToFormat(QImage::Format_RGB32);
            TEST_CHECK(!img.isNull(), "Test 531: image rendue");
            for (DT t : w.diagramTypes())
            {
                const int n = colorCount(img, D2::curveColor(t));
                std::cout << "  " << (layout == W::Layout::Overlay ? "superposées" : "empilées") << " : " << D2::shortName(t).toStdString() << " " << n << " px" << std::endl;
                TEST_CHECK(n > 300, "Test 531: courbe " << D2::shortName(t).toStdString() << " dessinée (" << n << " px)");
            }
        }
        std::cout << "[PASS] Test 531: Courbes 2D superposées (N/Vx, Vy, Vz…), échelles, valeurs et dessin" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 371 : aucun résultat résiduel dans la scène
    // -------------------------------------------------------------------------
    {
        Model m;
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        pump();
        const int empty = settledDisplayedObjects(view);
        auto results = portalResults(m);
        TEST_CHECK(results && results->isValid(), "Test 371: calcul du portique");
        view.rebuildAllShapes();
        pump();
        const int withModel = settledDisplayedObjects(view);   // vue stabilisée (BUG-174)
        auto* rv = view.resultsVisual();
        view.setResultsModel(results);
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        rv->setDeformedVisible(true);
        rv->setReactionsVisible(true);
        pump();
        const int shown = settledDisplayedObjects(view);
        const auto d1 = rv->diagnostics();
        std::cout << "  scène : vide " << empty << ", modèle " << withModel << ", résultats " << shown << " (diagrammes " << d1.diagrams << ", déformées " << d1.deformed
                  << ", réactions " << d1.reactions << ", étiquettes " << d1.diagramLabels + d1.reactionLabels << ")" << std::endl;
        TEST_CHECK(d1.diagrams == 3 && d1.deformed >= 3, "Test 371: un diagramme et une déformée par barre (poutre 1 et poteau 1 distincts) : " << d1.diagrams << " diagrammes");
        TEST_CHECK(shown == withModel + d1.total(), "Test 371: chaque objet de résultat affiché est connu du gestionnaire (aucun orphelin) : " << shown << " = " << withModel << " + " << d1.total());

        // Plusieurs calculs et changements de grandeur : aucun cumul d'objets
        for (int k = 0; k < 5; ++k)
        {
            view.setResultsModel(results);
            rv->setDiagramType(k % 2 ? TSA::Geometry::DiagramType::ShearForceVz : TSA::Geometry::DiagramType::BendingMy);
            pump(2);
        }
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        pump();
        TEST_CHECK(displayedObjects(view) == shown, "Test 371: aucun cumul après 5 calculs (" << displayedObjects(view) << " / " << shown << ")");

        // Absence de résultats (nouveau calcul en échec, projet ouvert) : la scène revient au modèle seul
        view.setResultsModel(nullptr);
        pump();
        TEST_CHECK(displayedObjects(view) == withModel && rv->diagnostics().total() == 0, "Test 371: résultats retirés (" << displayedObjects(view) << " / " << withModel << ")");

        // Nouveau projet : modèle vidé après un calcul affiché → scène vide (aucune courbe de l'ancien projet)
        view.setResultsModel(results);
        pump();
        view.setResultsModel(nullptr);
        m.clear();
        view.rebuildAllShapes();
        pump();
        TEST_CHECK(displayedObjects(view) == empty, "Test 371: nouveau projet — scène propre (" << displayedObjects(view) << " objets, vide : " << empty << ")");

        // Projet B, différent, calculé puis affiché ; aucune trace de A
        Model mb;
        view.setModel(&mb);
        auto rb = portalResults(mb, 4.5, 35.0);
        view.rebuildAllShapes();
        view.setResultsModel(rb);
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        pump();
        TEST_CHECK(rv->diagnostics().diagrams == 3 && displayedObjects(view) - rv->diagnostics().total() > 0, "Test 371: projet B affiché avec ses seuls résultats");
        view.setResultsModel(nullptr);
        view.setModel(nullptr);
        pump();
        view.hide();
        std::cout << "[PASS] Test 371: Aucun résultat résiduel (ids de familles distinctes, calculs successifs, nouveau projet)" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 372 : échelles distinctes, résultats inchangés
    // -------------------------------------------------------------------------
    {
        Model m;
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        auto results = portalResults(m);
        TEST_CHECK(results, "Test 372: calcul");
        view.rebuildAllShapes();
        auto* rv = view.resultsVisual();
        view.setResultsModel(results);
        // Valeurs de référence (copie) : aucune commande d'affichage ne doit les modifier
        const auto refDisp = results->allDisplacements();
        std::map<ElementKey, double> refMy;
        for (const auto& [k, e] : results->allElementResults()) refMy[k] = e.maxAbsMy();

        // Moment : crête automatique ≈ min(8 % de l'envergure, 60 % de la longueur médiane des barres)
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        pump();
        const double span = 6.0, medianL = 3.0;
        const double peak = rv->diagramMaxValue() * rv->diagramScale();
        TEST_CHECK(std::abs(peak - std::min(0.08 * span, 0.6 * medianL)) < 1e-9, "Test 372: crête du diagramme de moment " << peak << " m");
        // Flèche (mm) et rotation (rad, ~1e-3) : même hauteur à l'écran. L'ancien plafond (échelle ≤ 10 m par unité) donnait
        // une crête de quelques millimètres au diagramme de rotation.
        for (auto type : { TSA::Geometry::DiagramType::DeflectionUz, TSA::Geometry::DiagramType::RotationRy })
        {
            rv->setDiagramType(type);
            pump();
            const double peakU = rv->diagramMaxValue() * rv->diagramScale();
            std::cout << "  " << TSA::Geometry::DiagramGeometry::diagramTypeName(type).toStdString() << " : max " << rv->diagramMaxValue() << " "
                      << TSA::Geometry::DiagramGeometry::diagramUnit(type).toStdString() << ", crête affichée " << peakU << " m (ancienne : "
                      << rv->diagramMaxValue() * std::min(rv->diagramAutoScale(), 10.0) << " m)" << std::endl;
            TEST_CHECK(std::abs(peakU - peak) < 1e-9, "Test 372: diagramme de faible amplitude lisible (crête " << peakU << " m)");
        }
        // Légende : calcul affiché identifié ; étiquettes de rotation lisibles (4 chiffres significatifs)
        TEST_CHECK(rv->legendSummaryText().contains("CALCUL : OpenSees") && rv->diagnostics().diagramLabels > 0,
                   "Test 372: légende — calcul affiché identifié (" << rv->legendSummaryText().left(80).toStdString() << ")");
        // Multiplicateur manuel conservé après changement de grandeur et nouveau calcul
        rv->setDiagramScalePreset(TSA::Viewer::ScalePreset::Custom, 2.5);
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        view.setResultsModel(results);
        pump();
        TEST_CHECK(std::abs(rv->diagramScaleMultiplier() - 2.5) < 1e-12 && std::abs(rv->diagramMaxValue() * rv->diagramScale() - 2.5 * peak) < 1e-9,
                   "Test 372: multiplicateur ×2,5 conservé (crête " << rv->diagramMaxValue() * rv->diagramScale() << " m)");
        // Amplification de la déformée : sans effet sur l'échelle des diagrammes
        const double ds = rv->diagramScale();
        rv->setDeformedVisible(true);
        rv->setDeformationScalePreset(TSA::Viewer::ScalePreset::X1000);
        pump();
        TEST_CHECK(std::abs(rv->deformationScale() - 1000.0) < 1e-12 && std::abs(rv->diagramScale() - ds) < 1e-15, "Test 372: déformée ×1000 sans effet sur les diagrammes");
        // Zoom de la caméra : sans effet sur les échelles, les objets et les résultats
        const int objects = displayedObjects(view);
        view.view()->SetZoom(4.0);
        view.view()->SetZoom(0.1);
        pump();
        TEST_CHECK(std::abs(rv->diagramScale() - ds) < 1e-15 && std::abs(rv->deformationScale() - 1000.0) < 1e-12 && displayedObjects(view) == objects,
                   "Test 372: zoom de la caméra sans effet sur les échelles et la scène");
        bool same = results->allDisplacements().size() == refDisp.size();
        for (const auto& [id, d] : results->allDisplacements())
            same = same && refDisp.count(id) && refDisp.at(id).ux == d.ux && refDisp.at(id).uz == d.uz && refDisp.at(id).ry == d.ry;
        for (const auto& [k, e] : results->allElementResults()) same = same && refMy.at(k) == e.maxAbsMy();
        TEST_CHECK(same, "Test 372: résultats calculés identiques (bit à bit) après zoom, échelles et déformée");
        // Grandeur nulle partout (torsion d'un portique plan) : aucun diagramme, légende explicite
        rv->setDiagramScalePreset(TSA::Viewer::ScalePreset::Auto);
        rv->setDiagramType(TSA::Geometry::DiagramType::TorsionMx);
        pump();
        TEST_CHECK(rv->diagramAllZero() && rv->diagnostics().diagrams == 0 && rv->legendSummaryText().contains("nulle sur toute la structure"),
                   "Test 372: grandeur nulle annoncée, aucun diagramme arbitraire");
        view.setResultsModel(nullptr);
        view.setModel(nullptr);
        view.hide();
        std::cout << "[PASS] Test 372: Échelles de la caméra, des diagrammes et de la déformée distinctes ; résultats inchangés" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 373 : annotations à taille constante ou liée au modèle
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Viewer;
        auto& st = AnnotationStyle::instance();
        const AnnotationSettings saved = st.settings();   // réglages de l'utilisateur restaurés en fin de test
        st.reset();
        st.setDevicePixelRatio(1.0);
        TEST_CHECK(st.settings().mode == AnnotationSizeMode::ScreenConstant && std::abs(st.height(AnnotationRole::NodeLabel) - 12.0) < 1e-12,
                   "Test 373: valeur initiale — constante à l'écran, 12 px");
        st.setDevicePixelRatio(1.5);
        TEST_CHECK(std::abs(st.height(AnnotationRole::NodeLabel) - 18.0) < 1e-12 && std::abs(st.height(AnnotationRole::Extremum) - 22.5) < 1e-12,
                   "Test 373: facteur d'écran (DPI 150 %) et facteur de rôle");
        st.setBasePixels(4.0);
        const double lo = st.settings().basePixels;
        st.setBasePixels(99.0);
        TEST_CHECK(lo == 8.0 && st.settings().basePixels == 32.0, "Test 373: taille bornée à [8, 32] px");
        st.setMode(AnnotationSizeMode::ModelLinked);
        st.setModelHeight(0.3);
        st.setDevicePixelRatio(1.0);
        int scaleChanges = 0;
        auto scaleConn = QObject::connect(&st, &AnnotationStyle::changed, [&scaleChanges] { ++scaleChanges; });
        st.setViewScale(40.0);   // 0,3 m × ~40 px/m ≈ 12 px
        const double at40 = st.height(AnnotationRole::NodeLabel);
        st.setViewScale(40.5);   // même palier de 15 % : rien
        TEST_CHECK(scaleChanges == 1 && at40 > 10.0 && at40 < 14.0 && st.height(AnnotationRole::NodeLabel) == at40,
                   "Test 373: taille liée au modèle — mètres × échelle de la vue, paliers de 15 % (" << at40 << " px)");
        st.setViewScale(1.0);
        const double distant = st.height(AnnotationRole::NodeLabel);
        st.setViewScale(10000.0);
        TEST_CHECK(distant == 8.0 && st.height(AnnotationRole::NodeLabel) == 32.0, "Test 373: taille liée au modèle bornée à [8, 32] px");
        QObject::disconnect(scaleConn);
        // BUG-178 : jamais un texte OCCT « zoomable » (OCCT ne le trace pas sous ~16 unités : 0,3 m sortait haut de ~11 m)
        Handle(AIS_TextLabel) lbl = new AIS_TextLabel();
        st.apply(lbl, AnnotationRole::LoadValue);
        TEST_CHECK(!lbl->Attributes()->TextAspect()->Aspect()->GetTextZoomable() && lbl->Attributes()->TextAspect()->Height() == 32.0,
                   "Test 373: texte lié au modèle non zoomable, hauteur en pixels");
        st.setMode(AnnotationSizeMode::ScreenConstant);
        st.apply(lbl, AnnotationRole::LoadValue);
        TEST_CHECK(!lbl->Attributes()->TextAspect()->Aspect()->GetTextZoomable(), "Test 373: texte constant à l'écran non zoomable");
        st.adaptTo(400, 30.0);
        TEST_CHECK(st.settings().basePixels == 10.0 && std::abs(st.settings().modelHeight - 0.5) < 1e-12, "Test 373: adaptation automatique (petite vue, envergure 30 m)");

        // Vue réelle : changement de réglage → textes reconstruits, aucun objet orphelin ; zoom → rien de reconstruit
        Model m;
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        auto results = portalResults(m);
        view.rebuildAllShapes();
        view.setResultsModel(results);
        view.resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        pump();
        // Scène hors textes des résultats : leur nombre affiché suit le zoom (chevauchements masqués, test 606)
        auto sceneObjects = [&] {
            const auto d = view.resultsVisual()->diagnostics();
            return displayedObjects(view) - d.diagramLabels - d.reactionLabels;
        };
        const int before = sceneObjects();
        int changes = 0;
        QObject::connect(&st, &AnnotationStyle::changed, [&changes] { ++changes; });
        st.setMode(AnnotationSizeMode::ModelLinked);
        pump();
        TEST_CHECK(changes == 1 && sceneObjects() == before, "Test 373: mode changé — textes reconstruits sans cumul (" << displayedObjects(view) << " / " << before << ")");
        st.setMode(AnnotationSizeMode::ScreenConstant);
        pump();
        view.view()->SetZoom(3.0);
        view.zoomIn(1.0);
        pump(40);
        TEST_CHECK(changes == 2 && sceneObjects() == before, "Test 373: taille constante — zoom sans reconstruction");
        st.setMode(AnnotationSizeMode::ModelLinked);
        // hauteur de 10 px à l'échelle actuelle : 30 px après un zoom × 3 (sous le plafond de 32 px)
        st.setModelHeight(10.0 * view.devicePixelRatioF() / std::max(1e-6, view.viewPixelsPerMeter()));
        pump();
        const int beforeZoom = changes;
        const double heightBeforeZoom = st.height(AnnotationRole::NodeLabel);
        const double scaleBeforeZoom = view.viewPixelsPerMeter();
        view.zoomIn(3.0);
        pump(40);
        const int afterZoom = changes;
        TEST_CHECK(afterZoom == beforeZoom + 1 && st.height(AnnotationRole::NodeLabel) > heightBeforeZoom &&
                       view.viewPixelsPerMeter() > 2.0 * scaleBeforeZoom &&
                       sceneObjects() == before,
                   "Test 373: taille liée au modèle — zoom arrêté : textes agrandis une fois, zoom conservé, sans cumul (" << changes - beforeZoom << " reconstruction(s), avant " << heightBeforeZoom << " px, " << displayedObjects(view) << " / " << before << " objets, " << view.viewPixelsPerMeter() << " px/m, " << st.height(AnnotationRole::NodeLabel) << " px)");
        pump(40);
        TEST_CHECK(changes == afterZoom, "Test 373: caméra immobile — aucune reconstruction");
        st.setMode(saved.mode);
        st.setBasePixels(saved.basePixels);
        st.setModelHeight(saved.modelHeight);
        view.setResultsModel(nullptr);
        view.setModel(nullptr);
        view.hide();
        std::cout << "[PASS] Test 373: Annotations constantes à l'écran ou liées au modèle (DPI, bornes, rôles)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 374 : échelle des charges proportionnelle, automatique, normalisée
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Geometry;
        LoadScaleSettings s;
        s.mode = LoadScaleMode::Proportional;
        const double ref = 2.0;
        const double l10 = loadArrowLength(10, 100, ref, s), l50 = loadArrowLength(50, 100, ref, s), l100 = loadArrowLength(100, 100, ref, s);
        TEST_CHECK(std::abs(l10 - 0.2) < 1e-12 && std::abs(l50 - 1.0) < 1e-12 && std::abs(l100 - 2.0) < 1e-12, "Test 374: proportionnel — 10, 50, 100 kN → 0,2, 1, 2 m");
        TEST_CHECK(loadArrowLength(1000, 100, ref, s) == 2.0 && loadArrowLength(0.0, 100, ref, s) == 0.0 && std::abs(loadArrowLength(0.5, 100, ref, s) - 0.06) < 1e-12,
                   "Test 374: longueur maximale bornée, charge nulle sans flèche, minimum de visibilité 3 %");
        s.mode = LoadScaleMode::Automatic;
        TEST_CHECK(std::abs(loadArrowLength(10, 100, ref, s) - 0.5) < 1e-12 && std::abs(loadArrowLength(50, 100, ref, s) - 1.0) < 1e-12,
                   "Test 374: automatique — minimum garanti 25 %, rapports conservés au-dessus");
        s.mode = LoadScaleMode::Normalized;
        TEST_CHECK(loadArrowLength(10, 100, ref, s) == loadArrowLength(100, 100, ref, s), "Test 374: normalisé — longueurs égales");

        // Familles séparées : une charge linéique de 500 kN/m n'écrase pas les forces en kN
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(0, 0, 3);
        const int beam = m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, a, lc, 0, 0, -10.0));
        m.loadManager().addNodalLoad(NodalLoad(0, c, lc, 0, 0, -100.0));
        m.loadManager().addMemberLoad(MemberLoad::uniform(beam, lc, -500.0, LoadDirection::GlobalZ));
        const auto mx = loadFamilyMaxima(m.loadManager());
        TEST_CHECK(mx.of(LoadFamily::Force) == 100.0 && mx.of(LoadFamily::LineForce) == 500.0 && mx.of(LoadFamily::Pressure) == 0.0, "Test 374: maxima par famille");

        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        LoadScaleSettings ps;
        ps.mode = LoadScaleMode::Proportional;
        view.setLoadScaleSettings(ps);
        pump();
        const double f10 = view.loadArrowLengthFor(10, LoadFamily::Force), f100 = view.loadArrowLengthFor(100, LoadFamily::Force);
        std::cout << "  flèches : 10 kN → " << f10 << " m, 100 kN → " << f100 << " m ; légende : " << view.loadScaleLegendText().toStdString().substr(0, 60) << "…" << std::endl;
        TEST_CHECK(std::abs(f10 / f100 - 0.1) < 1e-12, "Test 374: vue — rapport 10/100 kN = 0,1");
        // Nouvelle charge plus forte : toute la famille est remise à l'échelle (reconstruction différée)
        m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 0, 0, -200.0));
        view.updateAllLoadShapes();
        pump(5);
        TEST_CHECK(std::abs(view.loadArrowLengthFor(100, LoadFamily::Force) / view.loadArrowLengthFor(200, LoadFamily::Force) - 0.5) < 1e-12 &&
                       view.loadFamilyMaxima().of(LoadFamily::Force) == 200.0,
                   "Test 374: nouvelle charge maximale appliquée à toute la famille");
        // Valeurs des charges inchangées par l'affichage
        double sumFz = 0;
        for (const auto& [id, nl] : m.loadManager().nodalLoads()) sumFz += nl.fz();
        TEST_CHECK(sumFz == -310.0 && m.loadManager().memberLoads().begin()->second.q1() == -500.0, "Test 374: valeurs des charges inchangées");
        TEST_CHECK(view.loadScaleLegendText().contains("kN/m") && view.loadScaleLegendText().contains("200"), "Test 374: légende par famille");
        view.setModel(nullptr);
        view.hide();
        std::cout << "[PASS] Test 374: Échelle des charges par famille (proportionnelle, automatique, normalisée)" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 583 : affichage intelligent des charges (2026-11-01) — longueur des flèches d'après la structure et jamais le
    // zoom, échelle robuste (charge hors norme), cas affichés sans toucher au modèle, couleurs et texte (style des charges)
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Geometry;
        using TSA::Viewer::LoadDisplayStyle;
        const QString savedOrg = QCoreApplication::organizationName(), savedApp = QCoreApplication::applicationName();
        QCoreApplication::setOrganizationName(QStringLiteral("TSA_TestSuite_Isolated"));
        QCoreApplication::setApplicationName(QStringLiteral("loads%1").arg(QCoreApplication::applicationPid()));
        auto& style = LoadDisplayStyle::instance();
        const TSA::Viewer::LoadDisplaySettings savedStyle = style.settings();
        style.reset();

        // Portique de deux étages de 3 m, travées de 6 m : longueur de référence = 35 % de la hauteur d'étage
        Model m;
        std::vector<int> base, l1, l2;
        for (double x : { 0.0, 6.0, 12.0 })
        {
            base.push_back(m.addNode(x, 0, 0));
            l1.push_back(m.addNode(x, 0, 3));
            l2.push_back(m.addNode(x, 0, 6));
        }
        std::vector<int> beams;
        for (std::size_t i = 0; i < 3; ++i)
        {
            m.addBar(base[i], l1[i], Section::ipe(300), Material::steelS235(), BarRole::Column);
            m.addBar(l1[i], l2[i], Section::ipe(300), Material::steelS235(), BarRole::Column);
        }
        for (std::size_t i = 0; i < 2; ++i)
        {
            beams.push_back(m.addBar(l1[i], l1[i + 1], Section::ipe(300), Material::steelS235(), BarRole::Beam));
            beams.push_back(m.addBar(l2[i], l2[i + 1], Section::ipe(300), Material::steelS235(), BarRole::Beam));
        }
        const double ref = structureLoadReferenceLength(m);
        TEST_CHECK(std::abs(ref - 1.05) < 1e-9, "Test 583: longueur de référence = 0,35 × 3 m d'étage (" << ref << " m)");
        TEST_CHECK(ref < 3.0 * 0.5, "Test 583: la flèche maximale tient sous le plancher supérieur");

        // Échelle robuste : dix charges de 10 kN et une de 1 000 kN
        Model r;
        const int lcR = r.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        for (int i = 0; i < 10; ++i) r.loadManager().addNodalLoad(NodalLoad(0, r.addNode(i, 0, 0), lcR, 0, 0, -10.0));
        r.loadManager().addNodalLoad(NodalLoad(0, r.addNode(20, 0, 0), lcR, 0, 0, -1000.0));
        const auto robust = loadFamilyMaxima(r.loadManager());
        TEST_CHECK(robust.of(LoadFamily::Force) == 1000.0 && robust.scaleOf(LoadFamily::Force) == 30.0,
                   "Test 583: charge hors norme — échelle sur 3 × 90e centile (" << robust.scaleOf(LoadFamily::Force) << ")");
        LoadScaleSettings automatic;
        TEST_CHECK(loadArrowLength(10, robust.scaleOf(LoadFamily::Force), 1.0, automatic) > 0.33 &&
                       loadArrowLength(1000, robust.scaleOf(LoadFamily::Force), 1.0, automatic) == 1.0,
                   "Test 583: les charges courantes ne sont plus écrasées (1/3 au lieu du minimum), la charge hors norme plafonnée");

        // Cas affichés : deux cas, le cas actif seul par défaut
        auto& lm = m.loadManager();
        const int g = lm.addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
        const int q = lm.addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        for (int b : beams)
        {
            lm.addMemberLoad(MemberLoad::uniform(b, g, -15.0, LoadDirection::GlobalZ));
            lm.addMemberLoad(MemberLoad::uniform(b, q, -8.0, LoadDirection::GlobalZ));
        }
        lm.setActiveLoadCaseId(g);
        m.markSaved();
        const auto revision = m.revision();
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        view.updateAllLoadShapes();
        pump();
        const std::size_t onlyG = view.displayedLoadObjectCount();
        TEST_CHECK(onlyG > 0 && view.loadCaseDisplayed(g) && !view.loadCaseDisplayed(q), "Test 583: cas actif seul affiché (" << onlyG << " objets)");
        TEST_CHECK(view.loadScaleLegendText().contains("Cas affiché : G") && view.loadScaleLegendText().contains("masqué"),
                   "Test 583: légende — cas affiché et cas masqués annoncés");
        view.setDisplayedLoadCase(q);
        pump();
        TEST_CHECK(lm.activeLoadCaseId() == q && view.loadCaseDisplayed(q) && !view.loadCaseDisplayed(g) && view.displayedLoadObjectCount() == onlyG,
                   "Test 583: autre cas affiché (même nombre d'objets)");
        TEST_CHECK(m.revision() == revision && !m.isModified(), "Test 583: changer de cas affiché ne touche ni la révision (résultats) ni « modifié »");
        auto s = style.settings();
        s.cases = TSA::Viewer::LoadCaseFilter::AllCases;
        style.setSettings(s);
        pump();
        TEST_CHECK(view.displayedLoadObjectCount() == 2 * onlyG, "Test 583: tous les cas superposés (" << view.displayedLoadObjectCount() << " objets)");

        // Indépendance au zoom : mêmes flèches et mêmes objets après un zoom
        const double before = view.loadArrowLengthFor(15.0, LoadFamily::LineForce);
        const std::size_t objects = view.displayedLoadObjectCount();
        view.zoomIn(3.0);
        pump();
        TEST_CHECK(view.loadArrowLengthFor(15.0, LoadFamily::LineForce) == before && view.displayedLoadObjectCount() == objects &&
                       std::abs(view.loadReferenceLength() - ref) < 1e-9,
                   "Test 583: zoom sans effet sur la longueur des flèches (" << before << " m)");

        // Texte des valeurs (BUG-177) : jamais un texte OCCT « zoomable » (une valeur de 0,25 m sortait haute de ~11 m et
        // recouvrait la structure) ; hauteur en mètres convertie en pixels à l'échelle de la vue, bornée [9, 16] px
        const double dpr = TSA::Viewer::AnnotationStyle::instance().settings().devicePixelRatio;
        TEST_CHECK(std::abs(style.labelPixels(10.0, ref) - 9.0) < 1e-9 && std::abs(style.labelPixels(40.0, ref) - 0.25 * ref * 40.0) < 1e-9 &&
                       std::abs(style.labelPixels(1000.0, ref) - 16.0) < 1e-9 && std::abs(style.labelPixels(40.0, ref, 2.0) - 18.0) < 1e-9 &&
                       std::abs(style.labelPixels(1000.0, ref, 2.0) - 32.0) < 1e-9,
                   "Test 583: hauteur à l'écran = 0,25 × réf. × px/m, bornée à [9, 16] px × facteur d'écran");
        view.fitAll();
        view.layoutLoadLabels();
        const auto fitted = view.loadLabelLayoutStats();
        TEST_CHECK(fitted.labels == static_cast<int>(2 * beams.size()) && !fitted.anyZoomable && fitted.minPixels >= 9.0 * dpr - 0.5 &&
                       fitted.maxPixels <= 16.0 * dpr + 0.5,
                   "Test 583: valeurs non zoomables, " << fitted.minPixels << " à " << fitted.maxPixels << " px (bornes 9–16 px × " << dpr << ")");
        view.zoomIn(4.0);
        view.layoutLoadLabels();
        TEST_CHECK(view.loadLabelLayoutStats().maxPixels >= fitted.maxPixels && view.loadLabelLayoutStats().maxPixels <= 16.0 * dpr + 0.5,
                   "Test 583: en zoomant, le texte grandit sans dépasser le plafond (" << view.loadLabelLayoutStats().maxPixels << " px)");
        auto screen = style.settings();
        screen.textSize = TSA::Viewer::LoadTextSize::ScreenConstant;
        style.setSettings(screen);
        pump();
        TEST_CHECK(std::abs(view.loadLabelLayoutStats().minPixels - std::round(12.0 * dpr)) < 0.5 &&
                       std::abs(view.loadLabelLayoutStats().maxPixels - std::round(12.0 * dpr)) < 0.5 && !view.loadLabelLayoutStats().anyZoomable,
                   "Test 583: texte constant à l'écran (12 px)");
        screen.textSize = TSA::Viewer::LoadTextSize::ModelLinked;
        style.setSettings(screen);
        pump();

        // Valeurs superposées (R+7 entièrement chargé, vue d'ensemble) : la plus forte reste, les autres sont masquées
        // dans la vue (sans être détruites) et la légende l'annonce ; en zoomant, elles réapparaissent
        {
            Model d;
            const int lcD = d.loadManager().addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
            for (int i = 0; i < 40; ++i) d.loadManager().addNodalLoad(NodalLoad(0, d.addNode(0.05 * i, 0, 0), lcD, 0, 0, -1.0 - i));
            d.addNode(60.0, 0, 30.0);   // grande scène : les 40 valeurs tiennent dans quelques pixels
            d.addNode(-60.0, 0, -30.0);
            OccView dv;
            dv.resize(640, 480);
            dv.show();
            pump(20);
            dv.setModel(&d);
            dv.updateAllLoadShapes();
            dv.fitAll();
            dv.layoutLoadLabels();
            pump();
            const auto dense = dv.loadLabelLayoutStats();
            TEST_CHECK(dense.labels == 40 && dense.hidden >= 30 && dv.displayedLoadObjectCount() >= 40 &&
                           dv.loadScaleLegendText().contains(QStringLiteral("masquée")),
                       "Test 583: valeurs superposées masquées (" << dense.hidden << " / " << dense.labels << "), légende");
            auto off = style.settings();
            off.declutter = false;
            style.setSettings(off);
            pump();
            dv.layoutLoadLabels();
            TEST_CHECK(dv.loadLabelLayoutStats().hidden == 0, "Test 583: masquage désactivable (style des charges)");
            off.declutter = true;
            style.setSettings(off);
            pump();
            dv.setModel(nullptr);
            dv.hide();
        }

        // Couleurs et texte
        std::set<QRgb> colors;
        for (int i = 0; i < 10; ++i) colors.insert(LoadDisplayStyle::caseColor(i).rgb());
        TEST_CHECK(colors.size() == 10 && LoadDisplayStyle::intensityColor(0.0) != LoadDisplayStyle::intensityColor(1.0),
                   "Test 583: dix couleurs de cas distinctes, dégradé d'intensité");
        TEST_CHECK(style.formatValue("q", -12.5, "kN/m", "Q") == "q = -12.50 kN/m", "Test 583: texte par défaut (" << style.formatValue("q", -12.5, "kN/m", "Q").toStdString() << ")");
        s = style.settings();
        s.decimals = 1;
        s.showSymbol = false;
        s.showCaseName = true;
        style.setSettings(s);
        TEST_CHECK(style.formatValue("q", -12.5, "kN/m", "Q") == "-12.5 kN/m [Q]", "Test 583: texte réglé (décimales, sans symbole, nom du cas)");
        TEST_CHECK(std::abs(style.modelTextHeight(ref) - 0.25 * ref) < 1e-12, "Test 583: hauteur du texte automatique liée à la structure");
        s = style.settings();
        s.textMinPixels = 20.0;
        s.textMaxPixels = 10.0;
        style.setSettings(s);
        TEST_CHECK(style.settings().textMinPixels == 20.0 && style.settings().textMaxPixels == 20.0, "Test 583: bornes à l'écran ordonnées (max ≥ min)");

        view.setModel(nullptr);
        view.hide();
        style.setSettings(savedStyle);
        QSettings().clear();
        QCoreApplication::setOrganizationName(savedOrg);
        QCoreApplication::setApplicationName(savedApp);
        std::cout << "[PASS] Test 583: Affichage des charges — échelle d'après la structure (sans zoom), robuste, cas affichés, couleurs, "
                     "texte borné à l'écran et valeurs superposées masquées" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 375 : copier-coller entre projets (presse-papier commun)
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Model;
        auto& clip = StructuralClipboard::shared();
        TEST_CHECK(&clip == &StructuralClipboard::shared(), "Test 375: presse-papier unique pour toute l'application");
        int srcBeam = 0, srcCaseG = 0;
        {
            // Projet A (détruit avant le collage : le presse-papier ne garde aucune référence vers lui)
            Model a;
            const int n1 = a.addNode(2, 3, 0), n2 = a.addNode(2, 3, 4), n3 = a.addNode(8, 3, 4), n4 = a.addNode(8, 3, 0);
            a.getNode(n1)->setSupport(SupportDefinition::fixed());
            a.getNode(n4)->setSupport(SupportDefinition::pinned());
            a.addColumn(n1, n2, Section::heb(200), Material::steelS235(), 90.0);
            srcBeam = a.addBar(n2, n3, Section::ipe(300), Material::steelS355(), BarRole::Beam, 15.0);
            a.addColumn(n4, n3, Section::heb(200), Material::steelS235());
            const int w = a.addWall(n1, n4, 4.0, 0.25);
            a.getWall(w)->setMaterial(Material::concreteC30_37());
            a.addTrussMember(n1, n3, 0.08);
            const int n5 = a.addNode(2, 9, 4), n6 = a.addNode(8, 9, 4);
            const int slab = a.addSlab({ n2, n3, n6, n5 }, 0.22);
            a.getSlab(slab)->setMaterial(Material::concreteC30_37());
            auto& lm = a.loadManager();
            lm.clear();
            srcCaseG = lm.addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
            const int q = lm.addLoadCase(LoadCase(0, "Q bureaux", LoadCaseCategory::Live));
            lm.addNodalLoad(NodalLoad(0, n2, q, 12.5, 0.0, -100.0));
            lm.addMemberLoad(MemberLoad::uniform(srcBeam, srcCaseG, -7.25, LoadDirection::GlobalZ));
            SurfaceLoad sl;
            sl.targetKind = SurfaceTargetKind::Slab;
            sl.surfaceId = slab;
            sl.loadCaseId = q;
            sl.value = -2.5;
            lm.addSurfaceLoad(sl);

            ClipboardSelection sel;
            sel.beams = { srcBeam };
            sel.columns = { 1, 2 };
            sel.walls = { w };
            sel.trusses = { 1 };
            sel.slabs = { slab };
            ClipboardOptions opt;
            opt.loads = true;
            clip.copyFrom(a, sel, opt);
        }
        TEST_CHECK(clip.hasData() && clip.nodeCount() == 6 && clip.totalElementCount() == 6 && clip.loadCount() == 3,
                   "Test 375: copie — 6 nœuds, 6 éléments (poutre, 2 poteaux, voile, treillis, dalle), 3 charges (" << clip.nodeCount() << ", " << clip.totalElementCount() << ", " << clip.loadCount() << ")");

        // Projet B : contenu existant, cas « G » d'id différent, pas de cas « Q bureaux »
        Model b;
        auto& lmB = b.loadManager();
        lmB.clear();
        lmB.addLoadCase(LoadCase(0, "Vent", LoadCaseCategory::Wind));
        const int gB = lmB.addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
        const int e1 = b.addNode(0, 0, 0), e2 = b.addNode(5, 0, 0);
        b.addBar(e1, e2, Section::ipe(200), Material::steelS235(), BarRole::Beam);
        const std::size_t nodesBefore = b.nodes().size(), beamsBefore = b.beams().size(), casesBefore = lmB.loadCases().size();

        const auto r = clip.pasteUndoable(b, 100.0, 0.0, 0.0, "Coller");
        TEST_CHECK(r.complete && r.nodeIds.size() == 6 && r.elementCount() == 6 && r.loadCount() == 3, "Test 375: collage complet dans le projet B");
        TEST_CHECK(b.nodes().size() == nodesBefore + 6 && b.beams().size() == beamsBefore + 1 && b.columns().size() == 2 && b.walls().size() == 1 &&
                       b.trussMembers().size() == 1 && b.slabs().size() == 1,
                   "Test 375: éléments ajoutés au contenu existant de B");
        // Géométrie : coin minimal (2, 3, 0) placé en (100, 0, 0)
        const auto* pBeam = b.getBeam(r.elementMap.at({ ElementKind::Beam, srcBeam }).id);
        const auto* s = pBeam ? b.getNode(pBeam->startNodeId()) : nullptr;
        const auto* e = pBeam ? b.getNode(pBeam->endNodeId()) : nullptr;
        TEST_CHECK(s && e && s->x() == 100.0 && s->y() == 0.0 && s->z() == 4.0 && e->x() == 106.0, "Test 375: positions relatives conservées");
        TEST_CHECK(pBeam && pBeam->properties().section.name == Section::ipe(300).name && pBeam->properties().material.name == Material::steelS355().name &&
                       pBeam->properties().rotation == 15.0,
                   "Test 375: section, matériau et orientation de la poutre");
        const auto& col1 = b.columns().begin()->second;
        TEST_CHECK(col1.properties().rotation == 90.0, "Test 375: orientation du poteau");
        int fixedCount = 0, pinnedCount = 0;
        for (int nid : r.nodeIds)
        {
            fixedCount += b.getNode(nid)->support().isFixed();
            pinnedCount += b.getNode(nid)->support().isPinned();
        }
        TEST_CHECK(fixedCount == 1 && pinnedCount == 1, "Test 375: appuis copiés (encastrement, articulation)");
        const auto& wall = b.walls().begin()->second;
        const auto& slabB = b.slabs().begin()->second;
        TEST_CHECK(wall.thickness() == 0.25 && wall.height() == 4.0 && wall.material().name == Material::concreteC30_37().name &&
                       slabB.thickness() == 0.22 && slabB.material().name == Material::concreteC30_37().name && slabB.nodeIds().size() == 4,
                   "Test 375: voile et dalle complets (épaisseur, hauteur, matériau)");
        // Charges : « G » rapproché du cas existant, « Q bureaux » créé ; valeurs identiques
        TEST_CHECK(r.loadCaseMap.at(srcCaseG) == gB && r.createdLoadCaseIds.size() == 1 && lmB.loadCases().size() == casesBefore + 1 &&
                       lmB.getLoadCase(r.createdLoadCaseIds[0])->name() == "Q bureaux",
                   "Test 375: cas de charge rapprochés par nom (G existant, Q bureaux créé)");
        const auto* ml = lmB.getMemberLoad(r.memberLoadIds.at(0));
        const auto* nl = lmB.getNodalLoad(r.nodalLoadIds.at(0));
        const auto* su = lmB.getSurfaceLoad(r.surfaceLoadIds.at(0));
        TEST_CHECK(ml && ml->q1() == -7.25 && ml->elementId() == pBeam->id() && ml->loadCaseId() == gB && nl && nl->fz() == -100.0 && nl->fx() == 12.5 &&
                       su && su->value == -2.5 && su->surfaceId == slabB.id(),
                   "Test 375: charges collées sur les nouveaux éléments, valeurs identiques (−7,25 kN/m ; 12,5 / −100 kN ; −2,5 kN/m², Z global)");

        // Annulation : une seule étape retire tout le collage ; Rétablir le remet
        TEST_CHECK(b.canUndo() && b.undo() && b.nodes().size() == nodesBefore && b.beams().size() == beamsBefore && b.slabs().empty() && b.walls().empty() &&
                       lmB.loadCases().size() == casesBefore && lmB.memberLoads().empty(),
                   "Test 375: Ctrl+Z annule le collage entier");
        TEST_CHECK(b.redo() && b.nodes().size() == nodesBefore + 6 && b.slabs().size() == 1, "Test 375: Ctrl+Y rétablit le collage");

        // Position d'origine : mêmes coordonnées que dans le projet source
        Model c;
        const auto r2 = clip.pasteAtOrigin(c);
        double minX = 1e9, minY = 1e9, minZ = 1e9;
        for (const auto& [id, n] : c.nodes()) minX = std::min(minX, n.x()), minY = std::min(minY, n.y()), minZ = std::min(minZ, n.z());
        TEST_CHECK(r2.complete && minX == 2.0 && minY == 3.0 && minZ == 0.0, "Test 375: collage à la position d'origine");

        // Tout ou rien : dalle dont un nœud n'existe pas (modèle source incohérent) → rien n'est collé
        {
            Model bad;
            const int x1 = bad.addNode(0, 0, 0), x2 = bad.addNode(1, 0, 0), x3 = bad.addNode(1, 1, 0);
            const int sb = bad.addSlab({ x1, x2, x3 }, 0.2);
            bad.addBar(x1, x2, Section::ipe(200), Material::steelS235(), BarRole::Beam);
            bad.getSlab(sb)->setNodeIds({ x1, x2, x3, 999 });
            ClipboardSelection sel;
            sel.slabs = { sb };
            sel.beams = { 1 };
            clip.copyFrom(bad, sel);
        }
        const std::size_t nB = b.nodes().size(), bB = b.beams().size();
        const bool couldUndo = b.canUndo();
        const auto r3 = clip.pasteUndoable(b, 0, 0, 0, "Coller");
        TEST_CHECK(!r3.complete && !r3.error.empty() && r3.empty() && b.nodes().size() == nB && b.beams().size() == bB && b.canUndo() == couldUndo,
                   "Test 375: dépendance manquante — rien collé, historique inchangé (" << r3.error << ")");
        clip.clear();
        std::cout << "[PASS] Test 375: Copier-coller entre projets (dépendances, charges, annulation, tout ou rien)" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 376 : diagnostic d'affichage
    // -------------------------------------------------------------------------
    {
        Model m;
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        auto results = portalResults(m);
        view.rebuildAllShapes();
        view.setResultsModel(results);
        view.resultsVisual()->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        pump();
        const int scene = displayedObjects(view);
        const auto d = view.resultsVisual()->diagnostics();
        const QString text = view.displayDiagnosticsText();
        std::cout << text.toStdString() << std::endl;
        TEST_CHECK(text.contains(QString("Scène : %1 objet").arg(scene)) && text.contains(QString("Résultats affichés : %1 objet").arg(d.total())) && d.total() > 0,
                   "Test 376: comptes de la scène et des résultats conformes à la vue");
        TEST_CHECK(text.contains("Caméra") && text.contains("Déformée") && text.contains("Diagrammes") && text.contains("Annotations") &&
                       text.contains("kN/m") && text.contains("valeurs calculées et charges inchangées"),
                   "Test 376: échelles graphiques listées (caméra, déformée, diagrammes, annotations, charges par famille)");
        const double my = results->allElementResults().begin()->second.maxAbsMy();
        TEST_CHECK(displayedObjects(view) == scene && results->allElementResults().begin()->second.maxAbsMy() == my, "Test 376: lecture du diagnostic sans effet");
        view.setResultsModel(nullptr);
        view.setModel(nullptr);
        view.hide();
        std::cout << "[PASS] Test 376: Diagnostic d'affichage (scène, résultats, échelles)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TESTS 604-607 : plusieurs résultats à la fois, isolation et coupe 2D du plan, textes, déformée filaire, couleurs des charges
    // -------------------------------------------------------------------------
    {
        using DT = TSA::Geometry::DiagramType;
        using PV = OccView::PlaneViewMode;
        using TSA::Coordinate::WorkPlane;
        using TSA::Coordinate::WorkPlaneType;
        QSettings().remove("Display/planeViewMode");
        Model m;
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        auto results = portalResults(m);   // portique dans le plan XZ : poteaux en X = 0 et 6, poutre à Z = 3
        TEST_CHECK(results && results->isValid(), "Test 604: calcul du portique");
        view.rebuildAllShapes();
        view.setPlaneViewMode(PV::Section);
        auto* rv = view.resultsVisual();
        view.setResultsModel(results);
        rv->setDeformedVisible(false);
        rv->setReactionsVisible(true);
        pump();
        // Contrôle visuel : TSA_TEST_CAPTURE=<dossier> enregistre les images de la vraie vue OCCT
        auto capture = [&](const char* name) {
            const QString folder = qEnvironmentVariable("TSA_TEST_CAPTURE");
            if (folder.isEmpty()) return;
            view.resize(1100, 800);
            pump(20);
            view.fitModel();
            pump(20);
            rv->layoutLabels();
            pump(10);
            view.captureViewImage(1100, 800).save(QDir(folder).filePath(QString::fromLatin1(name) + QStringLiteral(".png")));
        };

        // 604 : grandeurs cumulées, chacune avec ses objets ; retirer l'une laisse les autres
        rv->setDiagramVisible(DT::BendingMy, true);
        rv->setDiagramVisible(DT::ShearForceVz, true);
        rv->setDiagramVisible(DT::AxialForceN, true);
        pump();
        TEST_CHECK(rv->diagramTypes() == (std::vector<DT>{ DT::BendingMy, DT::ShearForceVz, DT::AxialForceN }) && rv->diagnostics().diagrams == 9,
                   "Test 604: trois grandeurs, trois barres : 9 diagrammes (" << rv->diagnostics().diagrams << ")");
        const QString legend = rv->legendSummaryText();
        TEST_CHECK(legend.contains(QStringLiteral("(My)")) && legend.contains(QStringLiteral("(Vz)")) && legend.contains(QStringLiteral("(N)")),
                   "Test 604: chaque grandeur dans la légende");
        rv->setDiagramVisible(DT::ShearForceVz, false);
        pump();
        TEST_CHECK(rv->isDiagramVisible(DT::BendingMy) && !rv->isDiagramVisible(DT::ShearForceVz) && rv->diagnostics().diagrams == 6,
                   "Test 604: grandeur retirée, les autres restent");
        rv->setDiagramVisible(DT::BendingMy, true);
        TEST_CHECK(rv->diagnostics().diagrams == 6, "Test 604: grandeur déjà affichée : aucun doublon");
        rv->setDiagramType(DT::ShearForceVz);
        pump();
        TEST_CHECK(rv->diagramTypes() == std::vector<DT>{ DT::ShearForceVz } && rv->diagnostics().diagrams == 3, "Test 604: choix unique (panneau Résultats) conservé");
        rv->setDiagramType(DT::None);
        pump();
        TEST_CHECK(rv->diagramTypes().empty() && rv->diagnostics().diagrams == 0 && rv->diagramType() == DT::None, "Test 604: Masquer les diagrammes");
        rv->setDiagramTypes({ DT::BendingMy, DT::AxialForceN });
        pump();
        capture("res_1_multi_3d");
        std::cout << "[PASS] Test 604: Plusieurs résultats affichés ensemble" << std::endl;
        ++passed;

        // 605 : le plan isole aussi les résultats ; coupe 2D par défaut, ancien mode disponible
        auto planeAt = [&](double z) { view.setActiveWorkPlane(WorkPlane(WorkPlaneType::GlobalXY, "Niveau", z)); pump(); };
        TEST_CHECK(OccView::storedPlaneViewMode() == PV::Section, "Test 605: coupe 2D par défaut (aucun réglage enregistré : vue en coupe)");
        planeAt(3.0);
        view.setWorkPlaneIsolation(true, 1.0);
        pump();
        TEST_CHECK(rv->diagnostics().diagrams == 2 && rv->diagnostics().reactions == 0 && view.sectionCutCount() == 0,
                   "Test 605: plan à Z = 3 en 3D : résultats de la poutre seule, aucun résultat des poteaux ni réaction (" << rv->diagnostics().diagrams << ")");
        view.setWorkPlaneIsolation(false, 1.0);
        pump();
        TEST_CHECK(rv->diagnostics().diagrams == 6, "Test 605: isolation retirée : tous les résultats reviennent");

        view.setMode2D(true);
        pump();
        capture("res_2_plan_z3");
        TEST_CHECK(rv->diagnostics().diagrams == 2 && view.sectionCutCount() == 2,
                   "Test 605: plan 2D à Z = 3 : poutre dans le plan, coupe des deux poteaux (" << view.sectionCutCount() << ")");
        view.setPlaneViewMode(PV::Proximity);
        pump();
        TEST_CHECK(rv->diagnostics().diagrams == 6 && view.sectionCutCount() == 0, "Test 605: ancien mode conservé : poteaux entiers, sans coupe");
        view.setPlaneViewMode(PV::Section);
        pump();
        planeAt(1.5);
        capture("res_3_coupe_mi_hauteur");
        TEST_CHECK(rv->diagnostics().diagrams == 0 && rv->diagnostics().diagramLabels == 0 && view.sectionCutCount() == 2,
                   "Test 605: plan à mi-hauteur : coupe des deux poteaux seule, aucun résultat superposé");
        view.setMode2D(false);
        pump();
        TEST_CHECK(view.sectionCutCount() == 0, "Test 605: sortie du 2D : coupe retirée");
        planeAt(0.0);
        view.setWorkPlaneIsolation(true, 1.0);
        view.setActiveWorkPlane(WorkPlane(WorkPlaneType::GlobalXZ, "File 1", 0.0));
        pump();
        TEST_CHECK(rv->diagnostics().diagrams == 6, "Test 605: plan XZ du portique : tous ses résultats");
        view.setWorkPlaneIsolation(false, 1.0);
        pump();
        view.isolateElements([&] { TSA::Model::ElementSet s; s.beams.insert(m.beams().begin()->first); return s; }());
        pump();
        TEST_CHECK(rv->diagnostics().diagrams == 2, "Test 605: isolation par éléments : résultats de l'élément isolé");
        view.showAllElements();
        pump();
        TEST_CHECK(rv->diagnostics().diagrams == 6, "Test 605: tout afficher");
        std::cout << "[PASS] Test 605: Plan de travail : résultats isolés, coupe 2D par défaut, ancien mode conservé" << std::endl;
        ++passed;

        // 606 : textes des résultats au-dessus de la structure, à une taille bornée, hors de la section
        planeAt(3.0);
        view.setWorkPlaneIsolation(true, 1.0);
        rv->setDiagramTypes({ DT::BendingMy });
        pump();
        rv->layoutLabels();
        pump();
        const auto& beam = m.beams().begin()->second;
        const auto* n1 = m.getNode(beam.startNodeId());
        const auto* n2 = m.getNode(beam.endNodeId());
        const double dpr = TSA::Viewer::AnnotationStyle::instance().settings().devicePixelRatio;
        const auto& textStyle = TSA::Viewer::LoadDisplayStyle::instance().settings();
        int labels = 0, topmost = 0, sized = 0, clear = 0;
        AIS_ListOfInteractive shown;
        view.context()->DisplayedObjects(shown);
        for (const auto& object : shown)
        {
            const Handle(AIS_TextLabel) label = Handle(AIS_TextLabel)::DownCast(object);
            if (label.IsNull()) continue;
            const QString text = QString::fromUtf8(TCollection_AsciiString(label->Text()).ToCString());
            if (!text.contains(QStringLiteral("kN"))) continue;
            // Légendes de l'écran (cartouches à deux points, sur plusieurs lignes) : ce ne sont pas des valeurs posées sur la structure
            if (text.contains(QLatin1Char(':')) || text.contains(QLatin1Char('\n')))
            {
                std::cout << "  légende : " << text.left(50).toStdString() << std::endl;
                continue;
            }
            ++labels;
            topmost += label->ZLayer() == Graphic3d_ZLayerId_Topmost;
            const double h = label->Attributes()->TextAspect()->Height();
            sized += h >= textStyle.textMinPixels * dpr - 0.5 && h <= textStyle.textMaxPixels * dpr + 0.5;
            // Distance à l'axe de la poutre (X entre les nœuds) : au-delà de la demi-hauteur de la section
            const gp_Pnt p = label->Position();
            const double u = std::clamp((p.X() - n1->x()) / (n2->x() - n1->x()), 0.0, 1.0);
            clear += p.Distance(gp_Pnt(n1->x() + u * (n2->x() - n1->x()), n1->y(), n1->z())) >= 0.15;
        }
        TEST_CHECK(labels > 0 && topmost == labels, "Test 606: textes au-dessus de la structure (" << topmost << " / " << labels << ")");
        TEST_CHECK(sized == labels, "Test 606: taille bornée (" << sized << " / " << labels << ")");
        TEST_CHECK(clear == labels, "Test 606: textes hors de la section (" << clear << " / " << labels << ")");
        // Plan vu de dessus : My est perpendiculaire au plan, le texte passe sur le côté de la poutre au lieu de se projeter dessus
        view.setMode2D(true);
        pump();
        rv->layoutLabels();
        pump();
        capture("res_4_textes_en_plan");
        AIS_ListOfInteractive inPlan;
        view.context()->DisplayedObjects(inPlan);
        int sideLabels = 0, valueLabels = 0;
        for (const auto& object : inPlan)
        {
            const Handle(AIS_TextLabel) label = Handle(AIS_TextLabel)::DownCast(object);
            if (label.IsNull()) continue;
            const QString text = QString::fromUtf8(TCollection_AsciiString(label->Text()).ToCString());
            if (!text.contains(QStringLiteral("kN")) || text.contains(QLatin1Char(':')) || text.contains(QLatin1Char('\n'))) continue;
            ++valueLabels;
            sideLabels += std::abs(label->Position().Y() - n1->y()) >= 0.15;
        }
        TEST_CHECK(valueLabels > 0 && sideLabels == valueLabels, "Test 606: en plan, textes sur le côté de la poutre (" << sideLabels << " / " << valueLabels << ")");
        view.setMode2D(false);
        pump();
        view.setWorkPlaneIsolation(false, 1.0);
        planeAt(0.0);
        std::cout << "[PASS] Test 606: Textes des résultats : position et échelle" << std::endl;
        ++passed;

        // 608 : valeurs aux extrémités, reliées à leur point de courbe par une ligne de rappel
        rv->setReactionsVisible(false);
        planeAt(3.0);
        view.setWorkPlaneIsolation(true, 1.0);
        rv->setDiagramTypes({ DT::BendingMy });
        pump();
        view.fitModel();
        pump(20);
        rv->layoutLabels();
        pump();
        const auto& segments = rv->leaderSegments();
        const double scale = rv->diagramScale();
        int endA = 0, endB = 0, onCurve = 0;
        for (const auto& [curve, text] : segments)
        {
            endA += curve.X() < n1->x() + 1e-6;
            endB += curve.X() > n2->x() - 1e-6;
            // Le texte à l'autre bout de la ligne donne la valeur : point de la courbe = axe + valeur × échelle
            AIS_ListOfInteractive objects;
            view.context()->DisplayedObjects(objects);
            for (const auto& object : objects)
            {
                const Handle(AIS_TextLabel) label = Handle(AIS_TextLabel)::DownCast(object);
                if (label.IsNull() || label->Position().Distance(text) > 1e-9) continue;
                const double value = QString::fromUtf8(TCollection_AsciiString(label->Text()).ToCString()).section(QLatin1Char(' '), 0, 0).toDouble();
                const double offset = std::abs(curve.Z() - n1->z());
                onCurve += std::abs(offset - std::abs(value) * scale) <= 1e-3 * std::abs(value) * scale + 1e-9;
            }
        }
        TEST_CHECK(!segments.empty() && endA == 1 && endB == 1, "Test 608: une valeur reliée à chaque extrémité de la poutre (" << endA << ", " << endB << " sur " << segments.size() << ")");
        TEST_CHECK(onCurve == int(segments.size()), "Test 608: chaque ligne part du point de courbe de sa valeur (" << onCurve << " / " << segments.size() << ")");
        TEST_CHECK(rv->diagnostics().leaders == 1, "Test 608: lignes regroupées par grandeur");
        view.setWorkPlaneIsolation(false, 1.0);
        planeAt(0.0);
        rv->setDiagramTypes({ DT::BendingMy, DT::AxialForceN });
        pump();
        rv->layoutLabels();
        pump();
        capture("res_5_lignes_de_rappel");
        TEST_CHECK(rv->diagnostics().leaders == 2, "Test 608: une couleur de ligne par diagramme");
        TEST_CHECK(int(rv->leaderSegments().size()) == rv->diagnostics().diagramLabels, "Test 608: une ligne par valeur visible (" << rv->leaderSegments().size() << " / " << rv->diagnostics().diagramLabels << ")");
        rv->setDiagramLabelsVisible(false);
        pump();
        TEST_CHECK(rv->leaderSegments().empty() && rv->diagnostics().leaders == 0, "Test 608: valeurs masquées : aucune ligne");
        rv->setDiagramLabelsVisible(true);
        planeAt(1.5);
        view.setWorkPlaneIsolation(true, 1.0);
        pump();
        TEST_CHECK(rv->leaderSegments().empty(), "Test 608: plan sans élément : aucune ligne");
        view.setWorkPlaneIsolation(false, 1.0);
        planeAt(0.0);
        rv->setDiagramType(DT::None);
        pump();
        TEST_CHECK(rv->diagnostics().leaders == 0, "Test 608: diagrammes masqués : lignes retirées");
        std::cout << "[PASS] Test 608: Valeurs des extrémités, lignes de rappel vers la courbe" << std::endl;
        ++passed;

        // 609 : en vue 2D, un diagramme perpendiculaire au plan est rabattu dans le plan, son sens conservé
        {
            using G = TSA::Geometry::DiagramGeometry;
            const gp_Pnt a(0, 0, 0), b(6, 0, 0);
            const gp_Vec up(0, 0, 1), down(0, 0, -1), inPlane(0, 1, 0);
            TEST_CHECK(G::foldOffsetIntoPlane(a, b, up, gp_Dir(0, 0, 1)).IsEqual(gp_Vec(0, 1, 0), 1e-9, 1e-9), "Test 609: perpendiculaire au plan : rabattu");
            TEST_CHECK(G::foldOffsetIntoPlane(a, b, down, gp_Dir(0, 0, 1)).IsEqual(gp_Vec(0, -1, 0), 1e-9, 1e-9), "Test 609: sens conservé");
            TEST_CHECK(G::foldOffsetIntoPlane(a, b, inPlane, gp_Dir(0, 0, 1)).IsEqual(inPlane, 1e-9, 1e-9), "Test 609: déjà dans le plan : inchangé");
            TEST_CHECK(G::foldOffsetIntoPlane(a, gp_Pnt(0, 0, 3), up, gp_Dir(0, 0, 1)).IsEqual(up, 1e-9, 1e-9), "Test 609: barre perpendiculaire au plan : inchangé");
        }
        planeAt(3.0);
        rv->setDiagramTypes({ DT::BendingMy });
        pump();
        view.setMode2D(true);
        pump();
        rv->layoutLabels();
        pump();
        capture("res_6_diagramme_rabattu");
        const auto folded = rv->leaderSegments();
        int inThePlane = 0, positiveSide = 0, negativeSide = 0;
        for (const auto& [curve, text] : folded)
        {
            inThePlane += std::abs(curve.Z() - n1->z()) < 1e-6;
            positiveSide += curve.Y() > 1e-3;
            negativeSide += curve.Y() < -1e-3;
        }
        TEST_CHECK(!folded.empty() && inThePlane == int(folded.size()), "Test 609: en plan, courbe dans le plan (" << inThePlane << " / " << folded.size() << ")");
        TEST_CHECK(positiveSide > 0 && negativeSide > 0, "Test 609: valeurs positives et négatives de part et d'autre de la poutre (" << positiveSide << ", " << negativeSide << ")");
        TEST_CHECK(rv->legendSummaryText().contains(QStringLiteral("rabattus")), "Test 609: rabattement annoncé dans la légende");
        view.setMode2D(false);
        pump();
        rv->layoutLabels();
        int outOfPlane = 0;
        for (const auto& [curve, text] : rv->leaderSegments()) outOfPlane += std::abs(curve.Z() - n1->z()) > 1e-3;
        TEST_CHECK(outOfPlane > 0 && !rv->legendSummaryText().contains(QStringLiteral("rabattus")), "Test 609: retour en 3D : direction réelle du diagramme");
        // Élévation du portique : My est déjà dans le plan, rien n'est rabattu
        view.setActiveWorkPlane(WorkPlane(WorkPlaneType::GlobalXZ, "File 1", 0.0));
        view.setMode2D(true);
        pump();
        rv->layoutLabels();
        int inElevation = 0;
        for (const auto& [curve, text] : rv->leaderSegments()) inElevation += std::abs(curve.Y()) < 1e-6;
        TEST_CHECK(inElevation == int(rv->leaderSegments().size()) && !rv->leaderSegments().empty() && !rv->legendSummaryText().contains(QStringLiteral("rabattus")),
                   "Test 609: élévation : My déjà dans le plan, rien de rabattu");
        view.setMode2D(false);
        planeAt(0.0);
        rv->setDiagramType(DT::None);
        pump();
        std::cout << "[PASS] Test 609: Diagrammes rabattus dans le plan en vue 2D" << std::endl;
        ++passed;

        // 607 : déformée filaire ; couleurs par type de charge
        rv->setDiagramType(DT::None);
        rv->setReactionsVisible(false);
        rv->setDeformedVisible(true);
        pump();
        auto solidObjects = [&] {
            AIS_ListOfInteractive list;
            view.context()->DisplayedObjects(list);
            int n = 0;
            for (const auto& object : list)
                if (const Handle(AIS_Shape) s = Handle(AIS_Shape)::DownCast(object); !s.IsNull() && TopExp_Explorer(s->Shape(), TopAbs_FACE).More()) ++n;
            return n;
        };
        const int withSolids = solidObjects(), deformed = rv->diagnostics().deformed;
        rv->setDeformedRepresentation(TSA::Viewer::DeformedRepresentation::Wire);
        pump();
        TEST_CHECK(rv->diagnostics().deformed == deformed && solidObjects() == withSolids - deformed,
                   "Test 607: déformée filaire : mêmes barres, aucun volume (" << solidObjects() << " / " << withSolids << ")");
        rv->setDeformedRepresentation(TSA::Viewer::DeformedRepresentation::Solid);
        pump();
        TEST_CHECK(solidObjects() == withSolids, "Test 607: retour aux volumes");

        auto& style = TSA::Viewer::LoadDisplayStyle::instance();
        const auto saved = style.settings();
        auto colored = saved;
        colored.colors = TSA::Viewer::LoadColorMode::ByCategory;
        colored.categoryColors[1] = QColor(1, 2, 3);
        style.setSettings(colored);
        TEST_CHECK(style.loadColor(0, 0.5, TSA::Geometry::LoadFamily::Force, 1) == QColor(1, 2, 3), "Test 607: couleur choisie pour un type de charge");
        TEST_CHECK(style.loadColor(0, 0.5, TSA::Geometry::LoadFamily::Force, 0) != QColor(1, 2, 3), "Test 607: un autre type garde la sienne");
        TEST_CHECK(style.loadColor(0, 0.5, TSA::Geometry::LoadFamily::Force, -1) == colored.singleColor, "Test 607: type inconnu : couleur unique");
        style.setSettings(saved);
        view.setResultsModel(nullptr);
        view.setModel(nullptr);
        view.hide();
        std::cout << "[PASS] Test 607: Déformée filaire, couleurs par type de charge" << std::endl;
        ++passed;
    }

    return true;
}
