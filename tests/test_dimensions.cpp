// Suite « dimensions » : cotations 3D (tests 237-249). Valeurs calculées à partir des coordonnées du
// modèle, associativité aux nœuds, références invalides, annulation, persistance (chunk DIMS), anciens
// fichiers, outils de saisie, sélection, exclusion du calcul, performance, fenêtres.

#include "test_common.h"

#include "Analysis/CalculationSnapshot.h"
#include "Annotation/DimensionGeometry.h"
#include "Annotation/DimensionService.h"
#include "IO/TSAFile.h"
#include "IO/TSAFileFormat.h"
#include "Interaction/Tools/ModelingTool.h"
#include "Model/ModelDiff.h"
#include "UI/Dialogs/DimensionDialogs.h"
#include "Viewer/SelectionManager.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <QElapsedTimer>
#include <QLineEdit>

using namespace TSA::Annotation;

namespace
{
Dimension linear(MeasureAxis axis, const std::vector<std::array<double, 3>>& pts, std::array<double, 3> pos,
                 DimensionKind kind = DimensionKind::Linear)
{
    Dimension d;
    d.kind = kind;
    d.axis = axis;
    for (const auto& p : pts)
    {
        DimensionAnchor a;
        a.point = p;
        d.anchors.push_back(a);
    }
    d.position = pos;
    return d;
}

std::vector<gp_Pnt> points(const Dimension& d)
{
    std::vector<gp_Pnt> out;
    for (const auto& a : d.anchors) out.emplace_back(a.point[0], a.point[1], a.point[2]);
    return out;
}

Dimension nodeDimension(int a, int b, std::array<double, 3> pos, MeasureAxis axis = MeasureAxis::Aligned)
{
    Dimension d;
    d.kind = DimensionKind::Linear;
    d.axis = axis;
    DimensionAnchor x, y;
    x.nodeId = a;
    y.nodeId = b;
    d.anchors = { x, y };
    d.position = pos;
    return d;
}
} // namespace

bool runSuite_Dimensions(int& passed)
{
    const DimensionStyle style;

    // -------------------------------------------------------------------------
    // TEST 237 : données — JSON, lecture tolérante
    // -------------------------------------------------------------------------
    {
        DimensionSet set;
        set.style.unit = LengthUnit::Millimeter;
        set.style.decimals = 0;
        set.style.textInPlane = true;
        Dimension d = linear(MeasureAxis::X, { { 0, 0, 0 }, { 6, 0, 0 } }, { 3, -1, 0 });
        d.id = 4;
        d.anchors[1].nodeId = 12;
        d.anchors[1].orphaned = true;
        d.textOverride = "L = <>";
        d.color = "#00BCD4";
        set.items[4] = d;
        set.nextId = 5;
        std::string warn;
        const DimensionSet r = DimensionSet::fromJson(set.toJson(), &warn);
        TEST_CHECK(warn.empty() && r == set, "Test 237: aller-retour JSON identique");
        TEST_CHECK(DimensionSet::fromJson("").items.empty(), "Test 237: ancien projet → aucune cotation");
        warn.clear();
        TEST_CHECK(DimensionSet::fromJson("{oups", &warn).items.empty() && !warn.empty(), "Test 237: JSON illisible signalé");
        warn.clear();
        const auto partial = DimensionSet::fromJson(
            R"({"schema":1,"dimensions":[{"id":1,"kind":"linear","anchors":[{"node":-1,"point":[0,0,0]}],"position":[0,0,0]},
                {"id":2,"kind":"level","anchors":[{"node":-1,"point":[0,0,3]}],"position":[1,0,3]}]})", &warn);
        TEST_CHECK(partial.items.size() == 1 && partial.items.count(2) && partial.nextId == 3 && !warn.empty(),
                   "Test 237: cotation incomplète ignorée et signalée, les autres conservées");
        std::cout << "[PASS] Test 237: Données des cotations (JSON, lecture tolérante)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 238 : cotation alignée — barre inclinée en 3D, ligne de cote à la position choisie
    // -------------------------------------------------------------------------
    {
        Dimension d = linear(MeasureAxis::Aligned, { { 0, 0, 0 }, { 3, 4, 12 } }, { 1, -2, 0 });
        const auto l = computeLayout(d, points(d), style);
        TEST_CHECK(l.valid && l.values.size() == 1 && std::abs(l.values[0] - 13.0) < 1e-12, "Test 238: vraie grandeur 13 m (3-4-12)");
        TEST_CHECK(l.texts.size() == 1 && l.texts[0].text == "13.000 m", "Test 238: texte « 13.000 m »");
        TEST_CHECK(l.arrows.size() == 2, "Test 238: deux flèches");
        // Ligne de cote : passe par la position, parallèle aux points mesurés, même longueur.
        const auto& dimLine = l.segments.back();
        const gp_Vec lineDir(dimLine.first, dimLine.second);
        TEST_CHECK(std::abs(lineDir.Magnitude() - 13.0) < 1e-9 && lineDir.IsParallel(gp_Vec(3, 4, 12), 1e-9),
                   "Test 238: ligne de cote parallèle et de même longueur");
        const gp_Pnt P(1, -2, 0);
        const gp_Vec toP(dimLine.first, P);
        TEST_CHECK(toP.Crossed(lineDir).Magnitude() < 1e-9, "Test 238: ligne de cote passant par la position cliquée");
        std::cout << "[PASS] Test 238: Cotation alignée d'une barre inclinée" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 239 : cotations X, Y, Z, horizontale ; axe automatique de la cotation linéaire
    // -------------------------------------------------------------------------
    {
        const std::vector<std::array<double, 3>> pts = { { 1, 2, 3 }, { 4, 8, 11 } };
        auto value = [&](MeasureAxis a) {
            Dimension d = linear(a, pts, { 0, 0, 20 });
            return computeLayout(d, points(d), style).values.at(0);
        };
        TEST_CHECK(std::abs(value(MeasureAxis::X) - 3) < 1e-12 && std::abs(value(MeasureAxis::Y) - 6) < 1e-12 &&
                       std::abs(value(MeasureAxis::Z) - 8) < 1e-12 && std::abs(value(MeasureAxis::Horizontal) - std::sqrt(45.0)) < 1e-12,
                   "Test 239: ΔX = 3, ΔY = 6, ΔZ = 8, horizontale = √45");
        Dimension vertical = linear(MeasureAxis::Horizontal, { { 0, 0, 0 }, { 0, 0, 5 } }, { 1, 0, 0 });
        TEST_CHECK(!computeLayout(vertical, points(vertical), style).valid, "Test 239: horizontale nulle refusée (points à la verticale)");
        const gp_Pnt a(0, 0, 0), b(6, 0, 3);
        TEST_CHECK(autoLinearAxis(a, b, gp_Pnt(3, 0, 6)) == MeasureAxis::X, "Test 239: curseur au-dessus → cotation suivant X");
        TEST_CHECK(autoLinearAxis(a, b, gp_Pnt(9, 0, 1.5)) == MeasureAxis::Z, "Test 239: curseur sur le côté → cotation suivant Z");
        std::cout << "[PASS] Test 239: Cotations X, Y, Z et horizontale" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 240 : cotation angulaire
    // -------------------------------------------------------------------------
    {
        auto angle = [&](std::array<double, 3> b, std::array<double, 3> pos = { 0.7, 0.7, 0 }) {
            Dimension d = linear(MeasureAxis::Aligned, { { 0, 0, 0 }, { 2, 0, 0 }, b }, pos, DimensionKind::Angular);
            return computeLayout(d, points(d), style);
        };
        auto right = angle({ 0, 3, 0 });
        TEST_CHECK(right.valid && std::abs(right.values[0] - 90.0) < 1e-9 && right.texts[0].text == "90.0°", "Test 240: angle droit 90.0°");
        TEST_CHECK(std::abs(angle({ 1, 1, 0 }).values[0] - 45.0) < 1e-9, "Test 240: 45°");
        TEST_CHECK(std::abs(angle({ 0, 1, 1 }).values[0] - 90.0) < 1e-9, "Test 240: angle dans un plan incliné");
        auto flat = angle({ -1, 0, 0 });
        TEST_CHECK(flat.valid && std::abs(flat.values[0] - 180.0) < 1e-9, "Test 240: angle plat 180°");
        TEST_CHECK(!angle({ 3, 0, 0 }).valid && !angle({ 0, 0, 0 }).valid, "Test 240: bras confondus ou nul refusés");
        TEST_CHECK(right.arrows.size() == 2 && right.segments.size() >= 8, "Test 240: arc et deux flèches");
        std::cout << "[PASS] Test 240: Cotation angulaire" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 241 : cotation de niveau
    // -------------------------------------------------------------------------
    {
        Dimension d = linear(MeasureAxis::Z, { { 2, 1, 3.5 } }, { 3, 1, 4 }, DimensionKind::Level);
        TEST_CHECK(computeLayout(d, points(d), style).texts[0].text == "+3.500 m", "Test 241: +3.500 m");
        DimensionStyle ref = style;
        ref.levelReference = 3.5;
        TEST_CHECK(computeLayout(d, points(d), ref).texts[0].text == "±0.000 m", "Test 241: ±0.000 m à la référence");
        ref.levelReference = 5.0;
        TEST_CHECK(computeLayout(d, points(d), ref).texts[0].text == "-1.500 m", "Test 241: niveau négatif");
        TEST_CHECK(computeLayout(d, points(d), style).levelMarkers.size() == 1, "Test 241: symbole de niveau");
        std::cout << "[PASS] Test 241: Cotation de niveau" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 242 : cotations en chaîne et cumulées
    // -------------------------------------------------------------------------
    {
        const std::vector<std::array<double, 3>> pts = { { 0, 0, 0 }, { 3, 0.2, 0 }, { 7.5, 0, 0 } };
        Dimension chain = linear(MeasureAxis::Aligned, pts, { 0, -1, 0 }, DimensionKind::Chain);
        const auto lc = computeLayout(chain, points(chain), style);
        TEST_CHECK(lc.valid && lc.values.size() == 2 && std::abs(lc.values[0] - 3.0) < 1e-12 && std::abs(lc.values[1] - 4.5) < 1e-12,
                   "Test 242: chaîne 3.000 + 4.500 (projection sur la ligne de cote)");
        TEST_CHECK(lc.arrows.size() == 4 && lc.texts.size() == 2, "Test 242: flèches et textes par segment");
        Dimension cum = linear(MeasureAxis::Aligned, pts, { 0, -1, 0 }, DimensionKind::Cumulative);
        const auto lu = computeLayout(cum, points(cum), style);
        TEST_CHECK(lu.valid && lu.values.size() == 2 && std::abs(lu.values[1] - 7.5) < 1e-12 && lu.texts.front().text == "0" &&
                       lu.texts.back().text == "7.500 m",
                   "Test 242: cumulée 0, 3.000, 7.500 depuis l'origine");
        std::cout << "[PASS] Test 242: Cotations en chaîne et cumulées" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 243 : unités, décimales, arrondi, texte imposé
    // -------------------------------------------------------------------------
    {
        DimensionStyle s;
        s.unit = LengthUnit::Millimeter;
        s.decimals = 0;
        TEST_CHECK(formatLength(s, 6.0) == "6000 mm", "Test 243: 6000 mm");
        s.rounding = 5;
        TEST_CHECK(formatLength(s, 1.2373) == "1235 mm", "Test 243: arrondi au pas de 5 mm");
        s.unit = LengthUnit::Centimeter;
        s.rounding = 0;
        s.decimals = 1;
        s.showUnit = false;
        TEST_CHECK(formatLength(s, 0.1234) == "12.3", "Test 243: centimètres sans unité");
        TEST_CHECK(applyTextOverride("L = <> (<>)", "6.000 m") == "L = 6.000 m (6.000 m)" && applyTextOverride("", "x") == "x",
                   "Test 243: texte imposé avec la valeur");
        TEST_CHECK(formatLength(style, -0.0000001) == "0.000 m", "Test 243: pas de « -0.000 »");
        std::cout << "[PASS] Test 243: Unités, précision, arrondi, texte" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 244 : associativité — nœud déplacé, nœud supprimé, Annuler, réassociation
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(0, 4, 0);
        m.addBar(a, b, Section::ipe(200), Material::steelS235());
        const auto rev = m.revision();
        std::string err;
        const int id = addDimension(m, nodeDimension(a, b, { 3, -1, 0 }), &err);
        TEST_CHECK(id > 0 && err.empty() && m.revision() == rev && m.isModified(), "Test 244: création (révision inchangée)");
        TEST_CHECK(std::abs(layoutFor(m, m.dimensions().items.at(id), m.dimensions().style).values[0] - 6.0) < 1e-12, "Test 244: 6.000 m");
        TEST_CHECK(addDimension(m, nodeDimension(a, 999, { 0, 0, 0 }), &err) == 0 && !err.empty(), "Test 244: nœud inexistant refusé");
        TEST_CHECK(addDimension(m, nodeDimension(a, a, { 0, 0, 0 }), &err) == 0, "Test 244: points confondus refusés");

        m.getNode(b)->setCoordinates(9, 0, 0);
        m.notifyNodeModified(b);
        TEST_CHECK(std::abs(layoutFor(m, m.dimensions().items.at(id), m.dimensions().style).values[0] - 9.0) < 1e-12,
                   "Test 244: nœud déplacé → valeur recalculée (9.000 m)");
        TEST_CHECK(dimensionsReferencingNode(m, b) == std::vector<int>{ id }, "Test 244: cotations du nœud");

        m.pushUndoState("Supprimer");
        m.removeNode(b);
        const auto& orphan = m.dimensions().items.at(id);
        const auto lo = layoutFor(m, orphan, m.dimensions().style);
        TEST_CHECK(orphan.anchors[1].orphaned && lo.invalidReference && lo.valid && std::abs(lo.values[0] - 9.0) < 1e-12,
                   "Test 244: nœud supprimé → référence invalide signalée, dernière position conservée, pas de crash");
        TEST_CHECK(m.undo() && !m.dimensions().items.at(id).anchors[1].orphaned && m.getNode(b), "Test 244: Annuler rétablit nœud et association");
        m.removeNode(b);
        TEST_CHECK(reassociateAnchor(m, id, 1, c, &err) && !layoutFor(m, m.dimensions().items.at(id), m.dimensions().style).invalidReference &&
                       std::abs(layoutFor(m, m.dimensions().items.at(id), m.dimensions().style).values[0] - 4.0) < 1e-12,
                   "Test 244: réassociation à un autre nœud");
        m.removeNode(c);
        TEST_CHECK(removeInvalidDimensions(m) == 1 && m.dimensions().items.empty(), "Test 244: suppression des cotations invalides");
        TEST_CHECK(m.undo() && m.dimensions().items.size() == 1, "Test 244: Annuler la suppression");
        std::cout << "[PASS] Test 244: Associativité et références invalides" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 245 : annotations hors calcul
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 0, 3);
        m.addBar(a, b, Section::ipe(200), Material::steelS235());
        m.addColumn(b, c, Section::rectangular(0.3, 0.3), Material::concreteC25_30());
        const auto before = TSA::Analysis::CalculationSnapshot::capture(m);
        for (int i = 0; i < 20; ++i) addDimension(m, nodeDimension(a, c, { 0, -1.0 - i, 0 }));
        const auto after = TSA::Analysis::CalculationSnapshot::capture(m);
        TEST_CHECK(after.elementCount() == before.elementCount() && after.nodeCount() == before.nodeCount() && m.nodes().size() == 3,
                   "Test 245: cotations absentes du calcul (éléments, nœuds), aucun nœud créé");
        std::cout << "[PASS] Test 245: Cotations hors calcul de structure" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 246 : persistance — chunk DIMS, rechargement, anciens fichiers, différentiel Annuler
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0);
        addDimension(m, nodeDimension(a, b, { 2.5, -1, 0 }, MeasureAxis::X));
        auto st = m.dimensions().style;
        st.unit = LengthUnit::Millimeter;
        setDimensionStyle(m, st);
        const std::string path = "test_dimensions.tsa";
        TSA::IO::TSAFileWriter writer;
        std::string err;
        TEST_CHECK(writer.saveToFile(path, m, nullptr, "Cotes", "TSA Testing", &err), "Test 246: sauvegarde");
        TSA::IO::TSAFileHeader header;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(path, header) && header.versionMinor == 6, "Test 246: format 1.6");
        Model loaded;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(path, loaded, nullptr, "", nullptr, nullptr, nullptr, &err) && loaded.dimensions() == m.dimensions(),
                   "Test 246: cotations et style rechargés à l'identique");
        loaded.getNode(b)->setCoordinates(8, 0, 0);
        TEST_CHECK(std::abs(layoutFor(loaded, loaded.dimensions().items.begin()->second, loaded.dimensions().style).values[0] - 8.0) < 1e-12,
                   "Test 246: association conservée après rechargement");
        Model old;
        old.addNode(0, 0, 0);
        const std::string oldPath = "test_dimensions_old.tsa";
        TEST_CHECK(writer.saveToFile(oldPath, old, nullptr, "Ancien", "TSA Testing", &err), "Test 246: projet sans cotation");
        Model oldLoaded;
        TEST_CHECK(reader.loadFromFile(oldPath, oldLoaded, nullptr, "", nullptr, nullptr, nullptr, &err) && oldLoaded.dimensions().items.empty(),
                   "Test 246: ancien projet ouvert sans cotation");
        const auto s1 = m.createSnapshot("a");
        removeDimensions(m, { m.dimensions().items.begin()->first });
        const auto s2 = m.createSnapshot("b");
        const auto diff = TSA::Model::ModelDiff::compute(s1, s2);
        TEST_CHECK(diff.dimensionsChanged && !diff.isEmpty() && diff.nodes.isEmpty(), "Test 246: différentiel Annuler « cotations modifiées »");
        std::filesystem::remove(path);
        std::filesystem::remove(oldPath);
        std::cout << "[PASS] Test 246: Persistance des cotations" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 247 : outils de saisie (accrochage aux nœuds, décalage tapé, chaîne, niveau)
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Interaction;
        ModelingToolRegistry reg;
        registerBuiltInModelingTools(reg);
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 4, 0);
        ToolContext ctx;
        auto pick = [](const gp_Pnt& p, int node = -1) {
            ToolPick k;
            k.point = p;
            k.nodeId = node;
            return k;
        };
        auto aligned = reg.create("dim_aligned");
        TEST_CHECK(aligned && aligned->category() == ToolCategory::Annotate && !aligned->supportsDialog(), "Test 247: outil de cotation 3D");
        aligned->addPick(pick(gp_Pnt(0, 0, 0), a), ctx);
        aligned->addPick(pick(gp_Pnt(6, 0, 0), b), ctx);
        TEST_CHECK(!aligned->ready() && !aligned->preview(gp_Pnt(3, -1, 0), ctx).lines.empty(), "Test 247: aperçu de la ligne de cote");
        ctx.cursor = gp_Pnt(3, -2, 0);
        TEST_CHECK(aligned->acceptValue(1.5, ctx) && aligned->ready(), "Test 247: décalage tapé au clavier");
        const auto res = aligned->apply(m, ctx);
        TEST_CHECK(res.success && m.dimensions().items.size() == 1, "Test 247: cotation créée");
        const auto& d = m.dimensions().items.begin()->second;
        TEST_CHECK(d.anchors[0].nodeId == a && d.anchors[1].nodeId == b && std::abs(d.position[1] + 1.5) < 1e-12,
                   "Test 247: associée aux nœuds accrochés, décalée de 1.5 m du côté du curseur");
        TEST_CHECK(aligned->pickCount() == 0 && !aligned->ready(), "Test 247: prêt pour la cotation suivante");

        auto chain = reg.create("dim_chain");
        chain->addPick(pick(gp_Pnt(0, 0, 0), a), ctx);
        chain->addPick(pick(gp_Pnt(6, 0, 0), b), ctx);
        chain->addPick(pick(gp_Pnt(9, 0, 0)), ctx);   // point fixe (grille)
        TEST_CHECK(chain->finish() && !chain->ready(), "Test 247: Entrée termine la saisie des points");
        chain->addPick(pick(gp_Pnt(0, -1, 0)), ctx);
        TEST_CHECK(chain->ready() && chain->apply(m, ctx).success, "Test 247: cotation en chaîne");
        const auto& dc = m.dimensions().items.rbegin()->second;
        TEST_CHECK(dc.kind == DimensionKind::Chain && dc.anchors.size() == 3 && dc.anchors[2].nodeId == -1, "Test 247: 3 points dont un point fixe");

        auto lin = reg.create("dim_linear");
        lin->addPick(pick(gp_Pnt(6, 0, 0), b), ctx);
        lin->addPick(pick(gp_Pnt(6, 4, 0), c), ctx);
        lin->addPick(pick(gp_Pnt(8, 2, 0)), ctx);
        TEST_CHECK(lin->apply(m, ctx).success && m.dimensions().items.rbegin()->second.axis == MeasureAxis::Y, "Test 247: linéaire → axe Y automatique");

        auto level = reg.create("dim_level");
        level->addPick(pick(gp_Pnt(6, 4, 0), c), ctx);
        level->addPick(pick(gp_Pnt(7, 5, 1)), ctx);
        TEST_CHECK(level->apply(m, ctx).success && m.dimensions().items.size() == 4, "Test 247: cotation de niveau");
        auto bad = reg.create("dim_angular");
        bad->addPick(pick(gp_Pnt(0, 0, 0)), ctx);
        bad->addPick(pick(gp_Pnt(1, 0, 0)), ctx);
        bad->addPick(pick(gp_Pnt(2, 0, 0)), ctx);
        bad->addPick(pick(gp_Pnt(0, 1, 0)), ctx);
        TEST_CHECK(!bad->apply(m, ctx).success && m.dimensions().items.size() == 4, "Test 247: angle dégénéré refusé sans modification");
        std::cout << "[PASS] Test 247: Outils de cotation" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 248 : sélection des cotations sans gêner celle des barres ; fenêtres
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0);
        const int beam = m.addBar(a, b, Section::ipe(200), Material::steelS235());
        const int dim = addDimension(m, nodeDimension(a, b, { 3, -1, 0 }));
        TSA::Viewer::SelectionManager sm;
        const TopoDS_Shape box = BRepPrimAPI_MakeBox(1, 1, 1).Shape();
        Handle(AIS_Shape) beamObj = new AIS_Shape(box), dimLines = new AIS_Shape(box), dimText = new AIS_Shape(box);
        sm.registerBeam(beam, beamObj);
        sm.registerDimension(dim, dimLines);
        sm.registerDimension(dim, dimText);
        TEST_CHECK(sm.isModelObject(beamObj) && !sm.isModelObject(dimText) && !sm.isModelObject(dimLines),
                   "Test 248: priorité au clic : la barre est un objet du modèle, la cotation non");
        sm.selectObject(dimText);
        TEST_CHECK(sm.currentSelectionType() == TSA::Viewer::SelectionType::Dimension && sm.selectedDimensions().count(dim) &&
                       sm.selectedElements().empty(),
                   "Test 248: clic sur le texte → cotation sélectionnée (aucun élément structurel)");
        sm.selectObject(beamObj);
        TEST_CHECK(sm.selectedBeams().count(beam) && sm.selectedDimensions().empty(), "Test 248: la barre reste sélectionnable");
        TEST_CHECK(sm.toggleObject(dimLines) && sm.selectedBeams().count(beam) && sm.selectedDimensions().count(dim),
                   "Test 248: Ctrl + clic ajoute la cotation à la sélection");
        removeDimensions(m, { dim });
        sm.pruneMissing(m);
        TEST_CHECK(sm.selectedDimensions().empty() && sm.selectedBeams().count(beam), "Test 248: cotation supprimée retirée de la sélection");

        const int d2 = addDimension(m, nodeDimension(a, b, { 3, -1, 0 }));
        TSA::UI::DimensionEditDialog dlg(&m, d2);
        auto* text = dlg.findChild<QLineEdit*>(QStringLiteral("dimensionText"));
        TEST_CHECK(text && dlg.findChild<QLineEdit*>(QStringLiteral("dimensionColor")), "Test 248: fenêtre de modification");
        text->setText("L = <>");
        QString err;
        TEST_CHECK(dlg.applyChanges(&err) && m.dimensions().items.at(d2).textOverride == "L = <>" &&
                       layoutFor(m, m.dimensions().items.at(d2), m.dimensions().style).texts[0].text == "L = 6.000 m",
                   "Test 248: texte imposé appliqué");
        DimensionStyle st;
        st.unit = LengthUnit::Centimeter;
        st.decimals = 1;
        TSA::UI::DimensionStyleDialog sdlg(st);
        TEST_CHECK(sdlg.style() == st, "Test 248: fenêtre de style (aller-retour)");
        std::cout << "[PASS] Test 248: Sélection des cotations et fenêtres" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 249 : performance avec de nombreuses cotations
    // -------------------------------------------------------------------------
    {
        Model m;
        std::vector<int> nodes;
        for (int i = 0; i < 200; ++i) nodes.push_back(m.addNode(i * 1.0, 0, (i % 7) * 0.5));
        DimensionSet set;
        for (int i = 0; i < 2000; ++i)
        {
            Dimension d = nodeDimension(nodes[i % 199], nodes[(i % 199) + 1], { i * 1.0, -1.0 - (i % 5), 0 },
                                        static_cast<MeasureAxis>(i % 5));
            d.id = i + 1;
            set.items[d.id] = d;
        }
        set.nextId = 2001;
        m.setDimensions(set);
        QElapsedTimer t;
        t.start();
        int valid = 0;
        for (const auto& [id, d] : m.dimensions().items) valid += layoutFor(m, d, m.dimensions().style).valid ? 1 : 0;
        const qint64 layoutMs = t.elapsed();
        t.restart();
        const auto roundTrip = DimensionSet::fromJson(m.dimensions().toJson());
        const qint64 jsonMs = t.elapsed();
        std::cout << "  2000 cotations : mise en page " << layoutMs << " ms, JSON aller-retour " << jsonMs << " ms" << std::endl;
        TEST_CHECK(valid >= 1900 && roundTrip == m.dimensions(), "Test 249: 2000 cotations valides et persistées");
        TEST_CHECK(layoutMs < 2000 && jsonMs < 3000, "Test 249: temps raisonnables (Debug)");
        std::cout << "[PASS] Test 249: Performance (2000 cotations)" << std::endl;
        ++passed;
    }
    return true;
}
