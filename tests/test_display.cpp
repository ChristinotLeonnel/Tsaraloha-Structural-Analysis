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
#include "UI/Diagrams/Diagram2DWidget.h"

#include <Prs3d_TextAspect.hxx>

#include <AIS_ListOfInteractive.hxx>
#include <Bnd_Box.hxx>
#include <QApplication>
#include <QImage>
#include <QLocale>
#include <QThread>

using namespace TSA::Analysis;

namespace
{
int displayedObjects(OccView& v)
{
    AIS_ListOfInteractive list;
    v.context()->DisplayedObjects(list);
    return list.Extent();
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
    m.loadManager().addMemberLoad(MemberLoad::uniform(beam, lc, load));
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
        const int empty = displayedObjects(view);
        auto results = portalResults(m);
        TEST_CHECK(results && results->isValid(), "Test 371: calcul du portique");
        view.rebuildAllShapes();
        pump();
        const int withModel = displayedObjects(view);
        auto* rv = view.resultsVisual();
        view.setResultsModel(results);
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        rv->setDeformedVisible(true);
        rv->setReactionsVisible(true);
        pump();
        const auto d1 = rv->diagnostics();
        const int shown = displayedObjects(view);
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
        TEST_CHECK(std::abs(st.height(AnnotationRole::NodeLabel) - 0.3) < 1e-12, "Test 373: taille liée au modèle en mètres");
        Handle(AIS_TextLabel) lbl = new AIS_TextLabel();
        st.apply(lbl, AnnotationRole::LoadValue);
        TEST_CHECK(lbl->Attributes()->TextAspect()->Aspect()->GetTextZoomable(), "Test 373: texte lié au modèle « zoomable »");
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
        const int before = displayedObjects(view);
        int changes = 0;
        QObject::connect(&st, &AnnotationStyle::changed, [&changes] { ++changes; });
        st.setMode(AnnotationSizeMode::ModelLinked);
        pump();
        TEST_CHECK(changes == 1 && displayedObjects(view) == before, "Test 373: mode changé — textes reconstruits sans cumul (" << displayedObjects(view) << " / " << before << ")");
        view.view()->SetZoom(3.0);
        pump();
        TEST_CHECK(changes == 1 && displayedObjects(view) == before, "Test 373: zoom sans reconstruction");
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
        m.loadManager().addMemberLoad(MemberLoad::uniform(beam, lc, 500.0));
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
        TEST_CHECK(sumFz == -310.0 && m.loadManager().memberLoads().begin()->second.q1() == 500.0, "Test 374: valeurs des charges inchangées");
        TEST_CHECK(view.loadScaleLegendText().contains("kN/m") && view.loadScaleLegendText().contains("200"), "Test 374: légende par famille");
        view.setModel(nullptr);
        view.hide();
        std::cout << "[PASS] Test 374: Échelle des charges par famille (proportionnelle, automatique, normalisée)" << std::endl;
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
            lm.addMemberLoad(MemberLoad::uniform(srcBeam, srcCaseG, 7.25));
            SurfaceLoad sl;
            sl.targetKind = SurfaceTargetKind::Slab;
            sl.surfaceId = slab;
            sl.loadCaseId = q;
            sl.value = 2.5;
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
        TEST_CHECK(ml && ml->q1() == 7.25 && ml->elementId() == pBeam->id() && ml->loadCaseId() == gB && nl && nl->fz() == -100.0 && nl->fx() == 12.5 &&
                       su && su->value == 2.5 && su->surfaceId == slabB.id(),
                   "Test 375: charges collées sur les nouveaux éléments, valeurs identiques (7,25 kN/m ; 12,5 / −100 kN ; 2,5 kN/m²)");

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
    
    return true;
}
