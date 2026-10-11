// Suite « demo » : trois modèles filaires de démonstration, construits avec l'API réelle de TSA (Model, Section,
// Material, GridManager, TSAFileWriter) et contrôlés (tests 347-350) :
//   A. pont en treillis Warren à montants, trois travées continues de 48 m, piles en palées métalliques, culées ;
//   B. hangar métallique 60 × 40 m : 11 portiques à fermes triangulées, pannes, lisses, contreventements, portes ;
//   C. bâtiment R+5 à ossature métallique : trame 5 × 3, poutres principales et secondaires, palées de stabilité,
//      escalier à volées et paliers, atrium, contreventement de toiture, édicule.
// Uniquement des barres (poutres, poteaux, montants, diagonales, contreventements) : aucune dalle, aucun voile.
// Contrôles : aucun nœud dupliqué, aucune barre dupliquée, superposée ou de longueur nulle, aucun nœud isolé, aucun
// nœud posé à l'intérieur d'une barre sans y être connecté, structure d'un seul tenant, appuis présents, calcul
// OpenSees réel sous poids propre (structure stable), enregistrement .tsa et relecture identique.
// Avec TSA_DEMO_DIR=<dossier>, les projets sont écrits dans ce dossier (vignette rendue par la vue 3D de TSA).

#include "test_common.h"

#include "Analysis/OpenSeesSolver.h"
#include "Grid/GridDefinition.h"
#include "Grid/GridManager.h"
#include "IO/TSAFile.h"
#include "Model/Load/LoadManager.h"
#include "Viewer/OccView.h"

#include <QThread>
#include <QTemporaryDir>
#include <iomanip>

#include <array>
#include <map>
#include <queue>
#include <set>

using namespace TSA::Analysis;

namespace
{
enum class Kind
{
    Column,   ///< élément vertical porteur (Column)
    Beam,     ///< poutre (BarRole::Beam)
    Truss,    ///< membrure, montant ou diagonale de treillis (BarRole::Truss)
    Brace     ///< contreventement (BarRole::Brace)
};

/// Construction d'un modèle filaire : nœuds uniques (coordonnées au millimètre), segments découpés à chaque nœud
/// situé sur eux (connexions garanties), paires de nœuds dédupliquées.
class WireBuilder
{
public:
    explicit WireBuilder(Model& model)
        : m_model(model)
    {
    }

    int node(double x, double y, double z)
    {
        const auto key = std::make_tuple(std::llround(x * 1000), std::llround(y * 1000), std::llround(z * 1000));
        if (auto it = m_nodes.find(key); it != m_nodes.end()) return it->second;
        const int id = m_model.addNode(x, y, z);
        m_nodes[key] = id;
        m_points[id] = gp_Pnt(x, y, z);
        return id;
    }

    void bar(const gp_Pnt& a, const gp_Pnt& b, Kind kind, const Section& section, const Material& material, const std::string& name)
    {
        m_segments.push_back({ node(a.X(), a.Y(), a.Z()), node(b.X(), b.Y(), b.Z()), kind, section, material, name });
    }

    /// Croix de Saint-André connectée en son centre : diagonales p1-p3 et p2-p4.
    void xBrace(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3, const gp_Pnt& p4, const Section& s, const Material& m, const std::string& name)
    {
        const gp_Pnt c((p1.X() + p3.X()) / 2, (p1.Y() + p3.Y()) / 2, (p1.Z() + p3.Z()) / 2);
        bar(p1, c, Kind::Brace, s, m, name);
        bar(c, p3, Kind::Brace, s, m, name);
        bar(p2, c, Kind::Brace, s, m, name);
        bar(c, p4, Kind::Brace, s, m, name);
    }

    void support(double x, double y, double z, const SupportDefinition& def) { m_model.getNode(node(x, y, z))->setSupport(def); }

    /// Crée les barres : chaque segment est découpé aux nœuds qui se trouvent sur lui ; une paire déjà créée
    /// (segments superposés) n'est créée qu'une fois.
    void finalize()
    {
        std::set<std::pair<int, int>> done;
        for (const auto& s : m_segments)
        {
            const gp_Pnt A = m_points[s.a], B = m_points[s.b];
            const gp_Vec AB(A, B);
            const double L2 = AB.SquareMagnitude();
            std::vector<std::pair<double, int>> chain { { 0.0, s.a }, { 1.0, s.b } };
            for (const auto& [id, p] : m_points)
            {
                if (id == s.a || id == s.b) continue;
                const gp_Vec AP(A, p);
                const double t = AP.Dot(AB) / L2;
                if (t <= 1e-9 || t >= 1.0 - 1e-9) continue;
                if (AP.Crossed(AB).Magnitude() / std::sqrt(L2) < 1e-4) chain.push_back({ t, id });
            }
            std::sort(chain.begin(), chain.end());
            for (std::size_t k = 0; k + 1 < chain.size(); ++k)
            {
                const int a = chain[k].second, b = chain[k + 1].second;
                if (!done.insert({ std::min(a, b), std::max(a, b) }).second) continue;
                if (s.kind == Kind::Column)
                {
                    // Poteau orienté de bas en haut
                    const bool up = m_points[a].Z() <= m_points[b].Z();
                    m_model.addColumn(up ? a : b, up ? b : a, s.section, s.material, 0.0, s.name);
                }
                else
                {
                    const BarRole role = s.kind == Kind::Truss ? BarRole::Truss : s.kind == Kind::Brace ? BarRole::Brace : BarRole::Beam;
                    m_model.addBar(a, b, s.section, s.material, role, 0.0, s.name);
                }
            }
        }
        m_segments.clear();
    }

private:
    struct Segment
    {
        int a, b;
        Kind kind;
        Section section;
        Material material;
        std::string name;
    };
    Model& m_model;
    std::map<std::tuple<long long, long long, long long>, int> m_nodes;
    std::map<int, gp_Pnt> m_points;
    std::vector<Segment> m_segments;
};

gp_Pnt P(double x, double y, double z) { return gp_Pnt(x, y, z); }

std::vector<double> range(double start, double step, int count)
{
    std::vector<double> v;
    for (int i = 0; i < count; ++i) v.push_back(start + step * i);
    return v;
}

// -------------------------------------------------------------------------------------------------------------
// A. Pont en treillis Warren à montants, 3 travées continues de 48 m (panneaux de 6 m), largeur 10 m, hauteur 7 m
// -------------------------------------------------------------------------------------------------------------
void buildBridge(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material steel = Material::steelS355();
    const Material concrete = Material::reinforcedConcrete();
    const double panel = 6.0, B = 10.0, H = 7.0;
    const int n = 24; // 3 × 48 m
    const Section chord = Section::heb(300), diag = Section::hea(240), post = Section::hea(200);
    const Section floorBeam = Section::ipe(400), stringer = Section::ipe(300), strut = Section::hea(160);
    const Section lateral = Section::angle(0.12, 0.12, 0.012, "L 120x120x12");
    const Section pierLeg = Section::pipe(0.9, 0.02, "Tube 900x20"), pierBrace = Section::pipe(0.32, 0.01, "Tube 323x10");

    for (double y : { 0.0, B })
    {
        const std::string side = y == 0.0 ? "amont" : "aval";
        for (int i = 0; i < n; ++i)
        {
            const double x0 = i * panel, x1 = x0 + panel;
            w.bar(P(x0, y, 0), P(x1, y, 0), Kind::Truss, chord, steel, "Membrure inférieure " + side);
            w.bar(P(x0, y, H), P(x1, y, H), Kind::Truss, chord, steel, "Membrure supérieure " + side);
            // Warren : diagonales alternées
            if (i % 2 == 0) w.bar(P(x0, y, 0), P(x1, y, H), Kind::Truss, diag, steel, "Diagonale " + side);
            else w.bar(P(x0, y, H), P(x1, y, 0), Kind::Truss, diag, steel, "Diagonale " + side);
        }
        for (int i = 0; i <= n; ++i) w.bar(P(i * panel, y, 0), P(i * panel, y, H), Kind::Column, post, steel, "Montant " + side);
    }
    for (int i = 0; i <= n; ++i)
    {
        const double x = i * panel;
        w.bar(P(x, 0, 0), P(x, B, 0), Kind::Beam, floorBeam, steel, "Pièce de pont");
        if (i > 0 && i < n) w.bar(P(x, 0, H), P(x, B, H), Kind::Beam, strut, steel, "Entretoise supérieure");
    }
    for (int i = 0; i < n; ++i)
    {
        const double x0 = i * panel, x1 = x0 + panel;
        for (double y : { 2.5, 5.0, 7.5 }) w.bar(P(x0, y, 0), P(x1, y, 0), Kind::Beam, stringer, steel, "Longeron");
        w.xBrace(P(x0, 0, 0), P(x0, B, 0), P(x1, B, 0), P(x1, 0, 0), lateral, steel, "Contreventement inférieur");
        w.xBrace(P(x0, 0, H), P(x0, B, H), P(x1, B, H), P(x1, 0, H), lateral, steel, "Contreventement supérieur");
    }
    // Portails d'extrémité : entretoise haute et goussets
    for (double x : { 0.0, n * panel })
    {
        w.bar(P(x, 0, H), P(x, B, H), Kind::Beam, strut, steel, "Portail d'extrémité");
        w.bar(P(x, 0, H - 2.0), P(x, 2.0, H), Kind::Brace, lateral, steel, "Gousset de portail");
        w.bar(P(x, B, H - 2.0), P(x, B - 2.0, H), Kind::Brace, lateral, steel, "Gousset de portail");
    }
    // Piles P1, P2 : palées de deux fûts tubulaires contreventés, hauteur 14 m
    for (double x : { 48.0, 96.0 })
    {
        for (double y : { 0.0, B })
        {
            w.bar(P(x, y, -14), P(x, y, 0), Kind::Column, pierLeg, steel, "Fût de pile");
            w.support(x, y, -14, SupportDefinition::fixed());
        }
        w.bar(P(x, 0, -7), P(x, B, -7), Kind::Beam, pierBrace, steel, "Traverse de pile");
        w.bar(P(x, 0, -14), P(x, B, -14), Kind::Beam, pierBrace, steel, "Longrine de pile");
        w.xBrace(P(x, 0, -14), P(x, B, -14), P(x, B, -7), P(x, 0, -7), pierBrace, steel, "Contreventement de pile");
        w.xBrace(P(x, 0, -7), P(x, B, -7), P(x, B, 0), P(x, 0, 0), pierBrace, steel, "Contreventement de pile");
    }
    // Culées C0, C3 : voile de poteaux béton sur semelle filante (barres)
    for (double x : { 0.0, n * panel })
    {
        for (double y : { 0.0, 2.5, 5.0, 7.5, B })
        {
            w.bar(P(x, y, -3), P(x, y, 0), Kind::Column, Section::rectangular(0.8, 0.8, "Poteau 80x80"), concrete, "Poteau de culée");
            w.support(x, y, -3, SupportDefinition::fixed());
        }
        w.bar(P(x, 0, -3), P(x, B, -3), Kind::Beam, Section::rectangular(1.2, 1.0, "Semelle 120x100"), concrete, "Semelle de culée");
    }
    w.finalize();

    grid.setXPositions({ 0.0, 48.0, 96.0, 144.0 });
    grid.setXLabels({ "C0", "P1", "P2", "C3" });
    grid.setYPositions({ 0.0, B });
    grid.setYLabels({ "Amont", "Aval" });
    grid.setZLevels({ -14.0, 0.0, H });
    grid.setZLabels({ "Fondations", "Tablier", "Membrure sup." });
}

// -------------------------------------------------------------------------------------------------------------
// B. Hangar 60 × 40 m : 11 portiques tous les 6 m, fermes triangulées (panneaux de 2,5 m), faîtage à 14 m
// -------------------------------------------------------------------------------------------------------------
double roofZ(double y) { return 10.5 + 3.5 * (1.0 - std::abs(y - 20.0) / 20.0); }

void buildHangar(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material steel = Material::steelS355();
    const double W = 40.0, Lbay = 6.0, zb = 9.0, ze = 10.5;
    const int bays = 10, panels = 16;
    const Section column = Section::heb(300), chord = Section::hea(200), web = Section::angle(0.1, 0.1, 0.01, "L 100x100x10");
    const Section purlin = Section::ipe(200), girt = Section::upn(160), brace = Section::pipe(0.114, 0.006, "Tube 114x6");
    const Section gablePost = Section::hea(220), tie = Section::pipe(0.0889, 0.005, "Tube 89x5");

    for (int f = 0; f <= bays; ++f)
    {
        const double x = f * Lbay;
        const std::string frame = "Portique " + std::to_string(f + 1);
        for (double y : { 0.0, W })
        {
            w.bar(P(x, y, 0), P(x, y, ze), Kind::Column, column, steel, frame + " — poteau");
            w.support(x, y, 0, SupportDefinition::fixed());
        }
        for (int j = 0; j < panels; ++j)
        {
            const double y0 = j * 2.5, y1 = y0 + 2.5;
            w.bar(P(x, y0, roofZ(y0)), P(x, y1, roofZ(y1)), Kind::Truss, chord, steel, frame + " — membrure supérieure");
            w.bar(P(x, y0, zb), P(x, y1, zb), Kind::Truss, chord, steel, frame + " — membrure inférieure");
            // Pratt : diagonales descendant vers le faîtage
            if (y1 <= 20.0) w.bar(P(x, y0, zb), P(x, y1, roofZ(y1)), Kind::Truss, web, steel, frame + " — diagonale");
            else w.bar(P(x, y0, roofZ(y0)), P(x, y1, zb), Kind::Truss, web, steel, frame + " — diagonale");
        }
        for (int j = 1; j < panels; ++j) w.bar(P(x, j * 2.5, zb), P(x, j * 2.5, roofZ(j * 2.5)), Kind::Truss, web, steel, frame + " — montant");
    }
    for (int f = 0; f < bays; ++f)
    {
        const double x0 = f * Lbay, x1 = x0 + Lbay;
        for (int j = 0; j <= panels; ++j)
        {
            const double y = j * 2.5;
            w.bar(P(x0, y, roofZ(y)), P(x1, y, roofZ(y)), Kind::Beam, purlin, steel, j == 0 || j == panels ? "Sablière" : "Panne");
        }
        w.bar(P(x0, 20, zb), P(x1, 20, zb), Kind::Beam, tie, steel, "Lierne de membrure inférieure");
        for (double y : { 0.0, W })
            for (double z : { 2.5, 5.0, 7.5 })
            {
                // Porte de long-pan (y = 0, x de 24 à 36 m) : pas de lisse basse, la lisse à 5 m sert de linteau
                if (y == 0.0 && z == 2.5 && x0 >= 24.0 && x1 <= 36.0) continue;
                w.bar(P(x0, y, z), P(x1, y, z), Kind::Beam, girt, steel, y == 0.0 ? "Lisse long-pan A" : "Lisse long-pan C");
            }
    }
    // Pignons : potelets aux nœuds de ferme (y = 10, 20, 30) et lisses ; grande porte au pignon x = 0 (y de 10 à 30 m)
    for (double x : { 0.0, bays * Lbay })
    {
        const bool door = x == 0.0;
        for (double y : { 10.0, 20.0, 30.0 })
        {
            const double z0 = door && y == 20.0 ? 7.5 : 0.0;
            w.bar(P(x, y, z0), P(x, y, zb), Kind::Column, gablePost, steel, "Potelet de pignon");
            if (z0 == 0.0) w.support(x, y, 0, SupportDefinition::pinned());
        }
        const std::vector<double> ys { 0.0, 10.0, 20.0, 30.0, W };
        for (std::size_t k = 0; k + 1 < ys.size(); ++k)
            for (double z : { 2.5, 5.0, 7.5 })
            {
                if (door && z < 7.5 && ys[k] >= 10.0 && ys[k + 1] <= 30.0) continue; // porte 20 × 7,5 m
                w.bar(P(x, ys[k], z), P(x, ys[k + 1], z), Kind::Beam, girt, steel, "Lisse de pignon");
            }
    }
    // Contreventements dans les travées d'extrémité : long-pans (croix sur deux panneaux) et toiture
    for (double x0 : { 0.0, (bays - 1) * Lbay })
    {
        const double x1 = x0 + Lbay;
        for (double y : { 0.0, W })
        {
            w.xBrace(P(x0, y, 0), P(x1, y, 0), P(x1, y, 5), P(x0, y, 5), brace, steel, "Palée de long-pan");
            w.xBrace(P(x0, y, 5), P(x1, y, 5), P(x1, y, 10), P(x0, y, 10), brace, steel, "Palée de long-pan");
        }
        for (int j : { 0, 2, 4, 6, 8, 10, 12, 14 })
        {
            const double y0 = j * 2.5, y2 = y0 + 5.0;
            w.xBrace(P(x0, y0, roofZ(y0)), P(x1, y0, roofZ(y0)), P(x1, y2, roofZ(y2)), P(x0, y2, roofZ(y2)), brace, steel, "Poutre au vent de toiture");
        }
    }
    w.finalize();

    grid.setXPositions(range(0.0, Lbay, bays + 1));
    grid.setXLabels({ "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11" });
    grid.setYPositions({ 0.0, 20.0, W });
    grid.setYLabels({ "A", "B", "C" });
    grid.setZLevels({ 0.0, zb, ze, 14.0 });
    grid.setZLabels({ "Sol", "Entrait", "Sablière", "Faîtage" });
}

// -------------------------------------------------------------------------------------------------------------
// C. Bâtiment R+5 : trame 6 m (A-F) × 7 m (1-4), RDC 4,5 m puis 3,5 m, ossature métallique
// -------------------------------------------------------------------------------------------------------------
void buildBuilding(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material steel = Material::steelS355();
    const std::vector<double> xs { 0, 6, 12, 18, 24, 30 }, ys { 0, 7, 14, 21 };
    const std::vector<double> zs { 0.0, 4.5, 8.0, 11.5, 15.0, 18.5, 22.0 };
    const std::vector<std::string> levels { "RDC", "R+1", "R+2", "R+3", "R+4", "R+5", "Toiture" };
    const Section colLow = Section::heb(300), colHigh = Section::heb(240), girderX = Section::ipe(400), girderY = Section::ipe(360);
    const Section joist = Section::ipe(270), brace = Section::pipe(0.1683, 0.008, "Tube 168x8"), stair = Section::upn(200);
    const Section roofBrace = Section::angle(0.1, 0.1, 0.01, "L 100x100x10");

    for (std::size_t k = 0; k + 1 < zs.size(); ++k)
        for (double x : xs)
            for (double y : ys) w.bar(P(x, y, zs[k]), P(x, y, zs[k + 1]), Kind::Column, k < 3 ? colLow : colHigh, steel, "Poteau " + levels[k]);
    for (double x : xs)
        for (double y : ys) w.support(x, y, 0, SupportDefinition::fixed());

    auto inAtrium = [](double x0, double y, std::size_t k) { return x0 == 6.0 && y == 3.5 && (k == 1 || k == 2); };
    auto inStairWell = [](double x0, double y) { return x0 == 18.0 && y == 10.5; };
    for (std::size_t k = 1; k < zs.size(); ++k)
    {
        const double z = zs[k];
        for (std::size_t i = 0; i + 1 < xs.size(); ++i)
        {
            for (double y : ys) w.bar(P(xs[i], y, z), P(xs[i + 1], y, z), Kind::Beam, girderX, steel, "Poutre principale " + levels[k]);
            for (double y : { 3.5, 10.5, 17.5 })
            {
                if (inAtrium(xs[i], y, k) || inStairWell(xs[i], y)) continue; // trémies : atrium double hauteur, cage d'escalier
                w.bar(P(xs[i], y, z), P(xs[i + 1], y, z), Kind::Beam, joist, steel, "Solive " + levels[k]);
            }
        }
        for (double x : xs)
            for (std::size_t j = 0; j + 1 < ys.size(); ++j) w.bar(P(x, ys[j], z), P(x, ys[j + 1], z), Kind::Beam, girderY, steel, "Poutre " + levels[k]);
    }
    // Palées de stabilité : façades 1 et 4 (travée C-D), pignons A et F (travée 2-3), sur toute la hauteur
    for (std::size_t k = 0; k + 1 < zs.size(); ++k)
    {
        const double z0 = zs[k], z1 = zs[k + 1];
        for (double y : { 0.0, 21.0 }) w.xBrace(P(12, y, z0), P(18, y, z0), P(18, y, z1), P(12, y, z1), brace, steel, "Palée de façade");
        for (double x : { 0.0, 30.0 }) w.xBrace(P(x, 7, z0), P(x, 14, z0), P(x, 14, z1), P(x, 7, z1), brace, steel, "Palée de pignon");
    }
    // Escalier (cage D-E / 2-3) : deux volées par niveau, palier intermédiaire contre l'axe E
    for (std::size_t k = 0; k + 2 < zs.size(); ++k)
    {
        const double z0 = zs[k], z1 = zs[k + 1], zm = (z0 + z1) / 2;
        w.bar(P(24, 7, zm), P(24, 14, zm), Kind::Beam, stair, steel, "Poutre palière");
        w.bar(P(22.5, 7, zm), P(22.5, 14, zm), Kind::Beam, stair, steel, "Rive de palier");
        for (double y : { 7.0, 14.0 }) w.bar(P(22.5, y, zm), P(24, y, zm), Kind::Beam, stair, steel, "Rive de palier");
        if (k > 0) w.bar(P(19.5, 7, z0), P(19.5, 14, z0), Kind::Beam, stair, steel, "Chevêtre de trémie");
        w.bar(P(19.5, 7, z1), P(19.5, 14, z1), Kind::Beam, stair, steel, "Chevêtre de trémie");
        for (double y : { 8.0, 9.5 }) w.bar(P(19.5, y, z0), P(22.5, y, zm), Kind::Beam, stair, steel, "Limon de volée");
        for (double y : { 11.5, 13.0 }) w.bar(P(22.5, y, zm), P(19.5, y, z1), Kind::Beam, stair, steel, "Limon de volée");
        if (k == 0)
        {
            // Départ de l'escalier au rez-de-chaussée : limons appuyés au sol
            w.bar(P(19.5, 8.0, 0), P(19.5, 9.5, 0), Kind::Beam, stair, steel, "Marche de départ");
            for (double y : { 8.0, 9.5 }) w.support(19.5, y, 0, SupportDefinition::pinned());
        }
    }
    // Toiture : poutres au vent horizontales dans les angles, édicule d'accès au-dessus de la cage d'escalier
    const double zr = zs.back();
    for (auto [x0, y0] : std::vector<std::pair<double, double>> { { 0, 0 }, { 24, 0 }, { 0, 14 }, { 24, 14 } })
        w.xBrace(P(x0, y0, zr), P(x0 + 6, y0, zr), P(x0 + 6, y0 + 7, zr), P(x0, y0 + 7, zr), roofBrace, steel, "Contreventement de toiture");
    const double zt = zr + 3.2;
    for (auto [x, y] : std::vector<std::pair<double, double>> { { 18, 7 }, { 24, 7 }, { 18, 14 }, { 24, 14 } })
        w.bar(P(x, y, zr), P(x, y, zt), Kind::Column, colHigh, steel, "Poteau d'édicule");
    w.bar(P(18, 7, zt), P(24, 7, zt), Kind::Beam, joist, steel, "Poutre d'édicule");
    w.bar(P(18, 14, zt), P(24, 14, zt), Kind::Beam, joist, steel, "Poutre d'édicule");
    w.bar(P(18, 7, zt), P(18, 14, zt), Kind::Beam, joist, steel, "Poutre d'édicule");
    w.bar(P(24, 7, zt), P(24, 14, zt), Kind::Beam, joist, steel, "Poutre d'édicule");
    w.xBrace(P(18, 7, zr), P(24, 7, zr), P(24, 7, zt), P(18, 7, zt), roofBrace, steel, "Contreventement d'édicule");
    w.xBrace(P(18, 7, zr), P(18, 14, zr), P(18, 14, zt), P(18, 7, zt), roofBrace, steel, "Contreventement d'édicule");
    w.finalize();

    grid.setXPositions(xs);
    grid.setXLabels({ "A", "B", "C", "D", "E", "F" });
    grid.setYPositions(ys);
    grid.setYLabels({ "1", "2", "3", "4" });
    grid.setZLevels(zs);
    grid.setZLabels(levels);
}

// --- Contrôles ----------------------------------------------------------------------------------------------
struct Topology
{
    std::size_t nodes = 0, beams = 0, columns = 0, supports = 0, duplicateNodes = 0, duplicateBars = 0, zeroLength = 0;
    std::size_t isolatedNodes = 0, nodesInsideBars = 0, components = 0;
    double length = 0.0;
};

Topology inspect(const Model& m)
{
    Topology t;
    t.nodes = m.nodes().size();
    t.beams = m.beams().size();
    t.columns = m.columns().size();
    std::map<std::tuple<long long, long long, long long>, int> coords;
    for (const auto& [id, n] : m.nodes())
    {
        if (++coords[std::make_tuple(std::llround(n.x() * 1000), std::llround(n.y() * 1000), std::llround(n.z() * 1000))] > 1) ++t.duplicateNodes;
        if (n.support().isSupported()) ++t.supports;
    }
    std::vector<std::pair<int, int>> bars;
    for (const auto& [id, b] : m.beams()) bars.push_back({ b.startNodeId(), b.endNodeId() });
    for (const auto& [id, c] : m.columns()) bars.push_back({ c.startNodeId(), c.endNodeId() });
    std::set<std::pair<int, int>> pairs;
    std::map<int, std::vector<int>> adj;
    for (auto [a, b] : bars)
    {
        if (!pairs.insert({ std::min(a, b), std::max(a, b) }).second) ++t.duplicateBars;
        const auto* na = m.getNode(a);
        const auto* nb = m.getNode(b);
        const gp_Pnt A(na->x(), na->y(), na->z()), B(nb->x(), nb->y(), nb->z());
        const double L = A.Distance(B);
        t.length += L;
        if (L < 1e-6) ++t.zeroLength;
        adj[a].push_back(b);
        adj[b].push_back(a);
        // Nœud à l'intérieur de la barre sans y être connecté (superposition ou connexion manquante)
        const gp_Vec AB(A, B);
        for (const auto& [id, n] : m.nodes())
        {
            if (id == a || id == b) continue;
            const gp_Vec AP(A, gp_Pnt(n.x(), n.y(), n.z()));
            const double s = AP.Dot(AB) / (L * L);
            if (s > 1e-9 && s < 1 - 1e-9 && AP.Crossed(AB).Magnitude() / L < 1e-4) ++t.nodesInsideBars;
        }
    }
    for (const auto& [id, n] : m.nodes())
        if (!adj.count(id)) ++t.isolatedNodes;
    std::set<int> seen;
    for (const auto& [id, n] : m.nodes())
    {
        if (seen.count(id) || !adj.count(id)) continue;
        ++t.components;
        std::queue<int> q;
        q.push(id);
        seen.insert(id);
        while (!q.empty())
        {
            const int u = q.front();
            q.pop();
            for (int v : adj[u])
                if (seen.insert(v).second) q.push(v);
        }
    }
    return t;
}

bool solveSelfWeight(Model& m, double* maxDisp, std::string* error)
{
    m.loadManager().addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead, true, 1.0));
    OpenSeesSolver solver;
    AnalysisParameters params;
    params.includeSelfWeight = true;
    QString e;
    const bool ok = solver.solveSynchronous(m, params, &e);
    if (!ok) *error = e.toStdString();
    *maxDisp = 0.0;
    for (const auto& [id, n] : m.nodes())
        if (const auto* d = solver.results().getNodeDisplacement(id)) *maxDisp = std::max(*maxDisp, d->translationMagnitude());
    return ok;
}

QImage thumbnailOf(Model& m)
{
    OccView view;
    view.resize(640, 480);
    view.show();
    for (int k = 0; k < 20; ++k)
    {
        QApplication::processEvents();
        QThread::msleep(5);
    }
    view.setModel(&m);
    view.viewIsometric();
    view.fitModel();
    for (int k = 0; k < 10; ++k) QApplication::processEvents();
    return view.captureViewImage(640, 480);
}

struct Demo
{
    const char* file;
    const char* title;
    void (*build)(Model&, TSA::Grid::GridDefinition&);
};
} // namespace

bool runSuite_DemoModels(int& passed)
{
    const Demo demos[] = {
        { "TSA_Demo_Pont_Treillis_3x48m.tsa", "Pont en treillis — 3 travées de 48 m", buildBridge },
        { "TSA_Demo_Hangar_60x40m.tsa", "Hangar métallique 60 × 40 m", buildHangar },
        { "TSA_Demo_Batiment_R+5.tsa", "Bâtiment R+5 à ossature métallique", buildBuilding },
    };
    const QString outDir = qEnvironmentVariable("TSA_DEMO_DIR");
    QTemporaryDir tmp;
    int testNo = 347;
    for (const auto& demo : demos)
    {
        Model m;
        TSA::Grid::GridDefinition def(demo.title, TSA::Grid::GridType::Cartesian);
        demo.build(m, def);
        const Topology t = inspect(m);
        std::cout << "  " << demo.title << " : " << t.nodes << " nœuds, " << t.beams + t.columns << " barres (" << t.columns << " poteaux/montants, "
                  << t.beams << " poutres, diagonales et contreventements), " << t.supports << " appuis, " << std::fixed << std::setprecision(0)
                  << t.length << " m de barres" << std::defaultfloat << std::endl;
        TEST_CHECK(t.nodes > 150 && t.beams + t.columns > 400, "Test " << testNo << ": modèle complexe (" << demo.title << ")");
        TEST_CHECK(m.slabs().empty() && m.walls().empty() && m.foundations().empty(), "Test " << testNo << ": uniquement des éléments filaires");
        TEST_CHECK(t.duplicateNodes == 0 && t.duplicateBars == 0 && t.zeroLength == 0,
                   "Test " << testNo << ": aucun doublon (nœuds " << t.duplicateNodes << ", barres " << t.duplicateBars << ", nulles " << t.zeroLength << ")");
        TEST_CHECK(t.isolatedNodes == 0 && t.nodesInsideBars == 0,
                   "Test " << testNo << ": connexions (nœuds isolés " << t.isolatedNodes << ", nœuds non connectés sur une barre " << t.nodesInsideBars << ")");
        TEST_CHECK(t.components == 1 && t.supports >= 4, "Test " << testNo << ": structure d'un seul tenant (" << t.components << "), " << t.supports << " appuis");
        // Calcul réel sous poids propre : la structure est stable (matrice non singulière)
        double umax = 0.0;
        std::string err;
        const bool solved = solveSelfWeight(m, &umax, &err); // ajoute le cas G (poids propre), enregistré avec le projet
        std::cout << "  calcul OpenSees sous poids propre : " << (solved ? "réussi" : "ÉCHEC " + err) << ", déplacement maximal " << std::fixed << std::setprecision(1) << umax * 1000 << std::defaultfloat << " mm" << std::endl;
        TEST_CHECK(solved && umax > 0 && umax < 0.2, "Test " << testNo << ": calcul sous poids propre (stabilité) — " << err);
        // Enregistrement au format natif et relecture
        TSA::Grid::GridManager grids;
        def.setVisible(true);
        def.setShowLabels(true);
        grids.clearAllGrids(); // le GridManager crée une grille par défaut : seule celle du modèle est enregistrée
        def.setActive(true);
        if (auto* g = grids.addGrid(def)) grids.setActiveGridId(g->id());
        TSA::IO::TSAFileWriter writer;
        writer.setThumbnail(thumbnailOf(m));
        const QString dir = outDir.isEmpty() ? tmp.path() : outDir;
        QDir().mkpath(dir);
        const std::string path = (dir + "/" + demo.file).toStdString();
        TEST_CHECK(writer.saveToFile(path, m, &grids, demo.title, "TSA — modèles de démonstration", &err), "Test " << testNo << ": enregistrement .tsa — " << err);
        Model back;
        TSA::Grid::GridManager backGrids;
        std::string name;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(path, back, &backGrids, "", &name, nullptr, nullptr, &err), "Test " << testNo << ": relecture — " << err);
        const Topology tb = inspect(back);
        std::cout << "  relu : " << tb.nodes << " nœuds, " << tb.beams << " poutres, " << tb.columns << " poteaux, " << tb.supports << " appuis, "
                  << backGrids.grids().size() << " grille(s), nom « " << name << " »" << std::endl;
        TEST_CHECK(tb.nodes == t.nodes && tb.beams == t.beams && tb.columns == t.columns && tb.supports == t.supports && name == demo.title &&
                       backGrids.grids().size() == 1,
                   "Test " << testNo << ": relecture identique (nœuds, barres, appuis, grille, nom)");
        if (!outDir.isEmpty()) std::cout << "  projet écrit : " << path << std::endl;
        std::cout << "[PASS] Test " << testNo << ": " << demo.title << std::endl;
        ++passed;
        ++testNo;
    }
    return true;
}
