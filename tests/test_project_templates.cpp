// Suite « templates_wp » : templates de projets et plans de travail dérivés de la géométrie réelle
// (Project/ProjectTemplates.h, Grid/GridCoordinateSync.h).
//   468  grille irrégulière saisie dans le désordre (A = 0, B = 4, C = 5, D = 3,2, E = 7) : identifiants attachés à leurs
//        coordonnées (grille et repère), plans X nommés par l'axe, accrochage sur les intersections réelles ;
//   469  X, Y et Z distingués : origine de la grille ajoutée, niveaux = objets Level (un plan par niveau, jamais par
//        intersection), coordonnées X / Y jamais proposées comme niveaux, niveaux proches non fusionnés, grille tournée ;
//   470  aucune valeur fixe : modèle neuf sans axe ni niveau ; anciens niveaux fixes {0 ; 3 ; 6,5 ; 10} remplacés par ceux
//        de la grille, niveaux d'un projet (étages IFC) conservés ; nouveau projet : niveaux de sa grille ;
//   471  actualisation : axes ajoutés, déplacés, supprimés → plans recalculés ; éléments structurels jamais déplacés ;
//   472  enregistrement puis réouverture d'un .tsa : mêmes plans, mêmes libellés, mêmes niveaux ;
//   473  « Enregistrer comme template » : fiche (nom, description), liste, projet source inchangé, nom en double refusé
//        sans confirmation, remplacement confirmé, nom invalide et dossier inaccessible refusés ;
//   474  nouveau projet depuis un template à grille irrégulière : copie indépendante (template inchangé), fichier
//        existant jamais écrasé, template corrompu refusé sans toucher au projet cible ;
//   475  compatibilité : template sans fiche listé sous le nom du fichier, ancien .tsa à niveaux fixes migré à
//        l'ouverture, gabarits intégrés inchangés.
#include "test_common.h"

#include "Coordinate/CoordinateSystem.h"
#include "Coordinate/LevelManager.h"
#include "Grid/GridCoordinateSync.h"
#include "Grid/GridDefinition.h"
#include "Grid/GridManager.h"
#include "Grid/GridSystem.h"
#include "IO/TSAFile.h"
#include "Model/Model.h"
#include "Project/ProjectManager.h"
#include "Project/ProjectTemplates.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>

using TSA::Coordinate::WorkPlaneAxis;
using TSA::Project::ProjectTemplateStore;

namespace
{
TSA::Grid::GridDefinition irregularGrid()
{
    // Ordre de saisie A, B, C, D, E ; ordre géométrique A, D, B, C, E
    TSA::Grid::GridDefinition def("Grille irrégulière", TSA::Grid::GridType::Cartesian);
    def.setXAxes({ 0.0, 4.0, 5.0, 3.2, 7.0 }, { "A", "B", "C", "D", "E" }, { false, false, false, true, false });
    def.setYAxes({ 0.0, 6.5, 2.5 }, { "1", "3", "2" });
    def.setZAxes({ 0.0, 3.2, 6.4 }, { "RDC", "R+1", "R+2" });
    return def;
}

bool hasPlane(const std::vector<TSA::Coordinate::DetectedPlaneInfo>& planes, double offset, const std::string& namePart = {})
{
    for (const auto& p : planes)
        if (std::abs(p.offset - offset) < 1e-9 && (namePart.empty() || p.name.find(namePart) != std::string::npos)) return true;
    return false;
}

QByteArray fileHash(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256) : QByteArray();
}

/// Projet de référence : grille irrégulière active, niveaux de la grille, deux nœuds.
void buildProject(TSA::Model::Model& model, TSA::Grid::GridManager& grids)
{
    grids.clearAllGrids();
    auto* g = grids.addGrid(irregularGrid());
    grids.setActiveGridId(g->id());
    TSA::Grid::applyGridLevels(g->definition(), *model.levelManager());
    TSA::Grid::syncCoordinateSystem(&g->definition(), *model.coordinateSystem());
    model.addNode(3.2, 2.5, 3.2);
    model.addNode(7.0, 6.5, 6.4);
    model.setModified(false);
}
} // namespace

bool runSuite_ProjectTemplates(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 468 : grille irrégulière, identifiants et coordonnées
    // -------------------------------------------------------------------------
    {
        const auto def = irregularGrid();
        TEST_CHECK((def.xPositions() == std::vector<double>{ 0.0, 3.2, 4.0, 5.0, 7.0 }), "Test 468: axes X triés par coordonnée");
        TEST_CHECK((def.xLabels() == std::vector<std::string>{ "A", "D", "B", "C", "E" }), "Test 468: chaque libellé reste attaché à sa coordonnée (D = 3,2)");
        TEST_CHECK(def.xIsBold(1) && !def.xIsBold(2), "Test 468: la mise en gras suit son axe (D)");
        TEST_CHECK((def.yLabels() == std::vector<std::string>{ "1", "2", "3" }) && std::abs(def.yPositions()[1] - 2.5) < 1e-12,
                   "Test 468: axes Y saisis dans le désordre");

        TSA::Model::Model model;
        TSA::Grid::syncCoordinateSystem(&def, *model.coordinateSystem());
        const auto xPlanes = model.detectStructuralPlanes(WorkPlaneAxis::X);
        TEST_CHECK(xPlanes.size() == 5 && hasPlane(xPlanes, 3.2, "D (X = 3.20 m)") && hasPlane(xPlanes, 4.0, "B (X = 4.00 m)"),
                   "Test 468: plans X nommés par l'identifiant de l'axe, à sa coordonnée réelle");
        double x = 0.0;
        int ix = -1;
        TEST_CHECK(model.coordinateSystem()->findClosestX(3.25, 0.1, x, ix) && std::abs(x - 3.2) < 1e-12
                       && model.coordinateSystem()->getXLabel(static_cast<size_t>(ix)) == "D",
                   "Test 468: accrochage sur l'axe réel D (X = 3,2), pas sur un pas régulier");
        TSA::Coordinate::Point3D snapped;
        int iy = -1, iz = -1;
        model.levelManager()->setFromElevations({ 0.0, 3.2 }, { "RDC", "R+1" });
        TEST_CHECK(model.coordinateSystem()->findClosestIntersection(TSA::Coordinate::Point3D(3.18, 2.53, 3.21), 0.1, snapped, ix, iy, iz)
                       && approxEqual(snapped.x, 3.2) && approxEqual(snapped.y, 2.5) && approxEqual(snapped.z, 3.2),
                   "Test 468: accrochage à l'intersection réelle D / 2 / R+1");
        std::cout << "[PASS] Test 468: Grille irrégulière saisie dans le désordre : identifiants, coordonnées réelles, accrochage" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 469 : X, Y et Z distingués
    // -------------------------------------------------------------------------
    {
        TSA::Model::Model model;
        auto def = irregularGrid();
        def.setOrigin(10.0, 20.0, 1.5);
        TSA::Grid::syncCoordinateSystem(&def, *model.coordinateSystem());
        TSA::Grid::applyGridLevels(def, *model.levelManager());
        TEST_CHECK(approxEqual(model.coordinateSystem()->xPositions().front(), 10.0) && approxEqual(model.coordinateSystem()->yPositions().back(), 26.5),
                   "Test 469: origine de la grille ajoutée aux axes X et Y");
        const auto zPlanes = model.detectStructuralPlanes(WorkPlaneAxis::Z);
        TEST_CHECK(zPlanes.size() == 3 && hasPlane(zPlanes, 1.5, "RDC") && hasPlane(zPlanes, 4.7, "R+1") && hasPlane(zPlanes, 7.9, "R+2"),
                   "Test 469: un plan Z par niveau (origine Z ajoutée), jamais un par intersection X-Y");
        TEST_CHECK(!hasPlane(zPlanes, 13.2) && !hasPlane(zPlanes, 22.5), "Test 469: coordonnées X et Y jamais proposées comme niveaux");
        TEST_CHECK(!hasPlane(model.detectStructuralPlanes(WorkPlaneAxis::X), 4.7), "Test 469: niveaux jamais proposés comme plans X");

        model.levelManager()->setFromElevations({ 0.0, 3.00, 3.04, 3.04 }, { "N0", "N1", "N1 bis", "doublon" });
        const auto close = model.detectStructuralPlanes(WorkPlaneAxis::Z);
        TEST_CHECK(hasPlane(close, 3.00, "N1") && hasPlane(close, 3.04, "N1 bis"), "Test 469: deux niveaux proches (4 cm) ne sont pas fusionnés");

        def.setRotationDeg(30.0);
        TSA::Grid::syncCoordinateSystem(&def, *model.coordinateSystem());
        TEST_CHECK(model.coordinateSystem()->xPositions().empty() && model.coordinateSystem()->yPositions().empty()
                       && model.coordinateSystem()->gridElevations().size() == 3,
                   "Test 469: grille tournée : ses axes ne sont pas des plans X = cte, ses altitudes restent proposées");
        std::cout << "[PASS] Test 469: Coordonnées X, Y et Z distinguées (niveaux, origine, grille tournée, niveaux proches)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 470 : aucune valeur fixe, migration des anciens niveaux
    // -------------------------------------------------------------------------
    {
        TSA::Model::Model fresh;
        TEST_CHECK(fresh.levelManager()->isEmpty() && fresh.coordinateSystem()->xPositions().empty(), "Test 470: modèle neuf sans axe ni niveau imposé");
        const auto zFresh = fresh.detectStructuralPlanes(WorkPlaneAxis::Z);
        TEST_CHECK(zFresh.size() == 1 && approxEqual(zFresh.front().offset, 0.0) && !hasPlane(zFresh, 6.5) && !hasPlane(zFresh, 10.0),
                   "Test 470: sans grille ni nœud, seul le plan de l'origine (aucune hauteur arbitraire)");

        TSA::Grid::GridManager grids;
        auto* g = grids.addGrid(irregularGrid());
        grids.setActiveGridId(g->id());
        TSA::Model::Model legacy;
        legacy.levelManager()->setFromElevations({ 0.0, 3.0, 6.5, 10.0 }, { "RDC (Z=0.00)", "Étage 1 (Z=3.00)", "Étage 2 (Z=6.50)", "Toiture (Z=10.00)" });
        TEST_CHECK(TSA::Grid::hasLegacyDefaultLevels(*legacy.levelManager()), "Test 470: anciens niveaux fixes reconnus");
        TSA::Grid::reconcileProject(legacy, &grids);
        TEST_CHECK((legacy.levelManager()->elevationList() == std::vector<double>{ 0.0, 3.2, 6.4 }) && legacy.levelManager()->levels()[1].name == "R+1",
                   "Test 470: anciens niveaux fixes remplacés par les altitudes de la grille");
        TEST_CHECK(legacy.coordinateSystem()->xLabels().size() == 5, "Test 470: axes du repère reconstruits depuis la grille active");

        TSA::Model::Model imported;
        imported.levelManager()->addLevel("Étage IFC 1", 0.0);
        imported.levelManager()->addLevel("Étage IFC 2", 2.75);
        TSA::Grid::reconcileProject(imported, &grids);
        TEST_CHECK(imported.levelManager()->levelCount() == 2 && approxEqual(imported.levelManager()->levels()[1].elevation, 2.75),
                   "Test 470: niveaux propres au projet (étages importés) conservés");

        TSA::Project::ProjectManager pm;
        TSA::Model::Model created;
        TSA::Grid::GridManager createdGrids;
        created.levelManager()->setFromElevations({ 99.0 });   // niveaux d'un projet précédent
        pm.newProject(created, &createdGrids);
        TEST_CHECK((created.levelManager()->elevationList() == std::vector<double>{ 0.0, 3.0, 6.0 })
                       && (created.coordinateSystem()->xPositions() == std::vector<double>{ 0.0, 5.0, 10.0, 15.0 }),
                   "Test 470: nouveau projet : niveaux et axes de sa grille par défaut (plus ceux du projet précédent)");
        std::cout << "[PASS] Test 470: Aucune valeur fixe ; anciens niveaux migrés, niveaux du projet conservés" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 471 : actualisation après modification de la grille
    // -------------------------------------------------------------------------
    {
        TSA::Model::Model model;
        TSA::Grid::GridManager grids;
        buildProject(model, grids);
        const auto nodesBefore = model.nodes();
        auto def = grids.activeGrid()->definition();
        def.setXAxes({ 0.0, 4.0, 5.0, 3.3, 7.0, 9.0 }, { "A", "B", "C", "D", "E", "F" });   // D déplacé, F ajouté
        grids.updateGrid(grids.activeGridId(), def);
        TSA::Grid::syncCoordinateSystem(&grids.activeGrid()->definition(), *model.coordinateSystem());
        auto xPlanes = model.detectStructuralPlanes(WorkPlaneAxis::X);
        TEST_CHECK(hasPlane(xPlanes, 3.3, "D (X = 3.30 m)") && hasPlane(xPlanes, 9.0, "F (X = 9.00 m)"), "Test 471: axe déplacé et axe ajouté repris");
        TEST_CHECK(hasPlane(xPlanes, 3.2, "Axe X = 3.20 m"), "Test 471: le nœud resté à X = 3,2 garde un plan (sans libellé d'axe)");
        def.setXAxes({ 0.0, 5.0 }, { "A", "C" });   // axes supprimés
        grids.updateGrid(grids.activeGridId(), def);
        TSA::Grid::syncCoordinateSystem(&grids.activeGrid()->definition(), *model.coordinateSystem());
        xPlanes = model.detectStructuralPlanes(WorkPlaneAxis::X);
        TEST_CHECK(!hasPlane(xPlanes, 9.0) && !hasPlane(xPlanes, 3.3) && hasPlane(xPlanes, 5.0, "C"), "Test 471: axes supprimés retirés des plans");
        bool unchanged = model.nodes().size() == nodesBefore.size();
        for (const auto& [id, n] : nodesBefore)
            unchanged = unchanged && model.nodes().count(id) && approxEqual(model.nodes().at(id).x(), n.x()) && approxEqual(model.nodes().at(id).z(), n.z());
        TEST_CHECK(unchanged, "Test 471: aucun élément structurel déplacé ni supprimé par la modification de la grille");
        std::cout << "[PASS] Test 471: Plans recalculés après modification de la grille ; éléments structurels conservés" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 472 : enregistrement et réouverture
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        TSA::Model::Model model;
        TSA::Grid::GridManager grids;
        buildProject(model, grids);
        const QString path = tmp.filePath(QStringLiteral("irregulier.tsa"));
        QString error;
        TEST_CHECK(TSA::IO::TSAProjectIO::saveProject(path, model, &grids, QStringLiteral("Irrégulier"), QString(), true, QImage(), &error),
                   "Test 472: enregistrement du projet");
        TSA::Project::ProjectManager pm;
        TSA::Model::Model reopened;
        TSA::Grid::GridManager reopenedGrids;
        TEST_CHECK(pm.openProject(path, reopened, &reopenedGrids, &error), "Test 472: réouverture du projet");
        const auto& d = reopenedGrids.activeGrid()->definition();
        TEST_CHECK((d.xLabels() == std::vector<std::string>{ "A", "D", "B", "C", "E" }) && approxEqual(d.xPositions()[1], 3.2),
                   "Test 472: axes et identifiants relus");
        const auto x = reopened.detectStructuralPlanes(WorkPlaneAxis::X), x0 = model.detectStructuralPlanes(WorkPlaneAxis::X);
        const auto z = reopened.detectStructuralPlanes(WorkPlaneAxis::Z), z0 = model.detectStructuralPlanes(WorkPlaneAxis::Z);
        bool same = x.size() == x0.size() && z.size() == z0.size();
        for (std::size_t i = 0; same && i < x.size(); ++i) same = x[i].name == x0[i].name && approxEqual(x[i].offset, x0[i].offset);
        for (std::size_t i = 0; same && i < z.size(); ++i) same = z[i].name == z0[i].name && approxEqual(z[i].offset, z0[i].offset);
        TEST_CHECK(same, "Test 472: mêmes plans X et Z (noms et positions) après réouverture");
        std::cout << "[PASS] Test 472: Enregistrement puis réouverture : grille, plans et niveaux identiques" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 473 : enregistrer comme template
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const ProjectTemplateStore store(tmp.filePath(QStringLiteral("templates")));
        TSA::Model::Model model;
        TSA::Grid::GridManager grids;
        buildProject(model, grids);
        QString error;
        TSA::Project::ProjectTemplateInfo saved;
        TEST_CHECK(store.save(QStringLiteral("Bâtiment R+2"), QStringLiteral("Grille irrégulière, trois niveaux"), model, &grids,
                              ProjectTemplateStore::Overwrite::Refuse, &error, &saved),
                   "Test 473: template enregistré");
        TEST_CHECK(QFile::exists(saved.path) && QFile::exists(QDir(store.directory()).filePath(QStringLiteral("Bâtiment R+2.json"))),
                   "Test 473: fichier .tsa et fiche du template écrits");
        TEST_CHECK(!model.isModified() && model.nodes().size() == 2, "Test 473: projet source inchangé (contenu et état « modifié »)");
        QStringList problems;
        const auto list = store.list(&problems);
        TEST_CHECK(list.size() == 1 && list.front().name == QStringLiteral("Bâtiment R+2") && list.front().description == QStringLiteral("Grille irrégulière, trois niveaux")
                       && problems.isEmpty(),
                   "Test 473: template listé avec son nom et sa description");
        const QByteArray before = fileHash(saved.path);
        TEST_CHECK(!store.save(QStringLiteral("  bâtiment r+2 "), QString(), model, &grids, ProjectTemplateStore::Overwrite::Refuse, &error)
                       && error.contains(QStringLiteral("existe déjà")) && fileHash(saved.path) == before,
                   "Test 473: nom en double refusé sans confirmation (casse et espaces ignorés), template intact");
        model.addNode(1.0, 1.0, 0.0);
        TEST_CHECK(store.save(QStringLiteral("Bâtiment R+2"), QStringLiteral("Mise à jour"), model, &grids, ProjectTemplateStore::Overwrite::Replace, &error)
                       && store.list().size() == 1 && store.list().front().description == QStringLiteral("Mise à jour") && fileHash(saved.path) != before,
                   "Test 473: remplacement confirmé (un seul template, contenu et fiche mis à jour)");
        TEST_CHECK(!store.save(QStringLiteral(" <>:*? "), QString(), model, &grids, ProjectTemplateStore::Overwrite::Refuse, &error), "Test 473: nom invalide refusé");
        QFile blocker(tmp.filePath(QStringLiteral("fichier")));
        TEST_CHECK(blocker.open(QIODevice::WriteOnly), "Test 473: préparation");
        blocker.close();
        const ProjectTemplateStore blocked(tmp.filePath(QStringLiteral("fichier/templates")));
        TEST_CHECK(!blocked.save(QStringLiteral("X"), QString(), model, &grids, ProjectTemplateStore::Overwrite::Refuse, &error) && !error.isEmpty(),
                   "Test 473: dossier impossible à créer : erreur explicite");
        std::cout << "[PASS] Test 473: Enregistrer comme template (fiche, liste, source inchangée, doublons, erreurs)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 474 : nouveau projet depuis un template
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const ProjectTemplateStore store(tmp.filePath(QStringLiteral("templates")));
        TSA::Model::Model model;
        TSA::Grid::GridManager grids;
        buildProject(model, grids);
        QString error;
        TSA::Project::ProjectTemplateInfo info;
        TEST_CHECK(store.save(QStringLiteral("Irrégulier"), QString(), model, &grids, ProjectTemplateStore::Overwrite::Refuse, &error, &info), "Test 474: template");
        const QByteArray templateHash = fileHash(info.path);

        const QString projectPath = tmp.filePath(QStringLiteral("Nouveau.tsa"));
        TEST_CHECK(store.createProject(info, projectPath, QStringLiteral("Nouveau"), &error), "Test 474: projet créé depuis le template");
        TSA::Project::ProjectManager pm;
        TSA::Model::Model project;
        TSA::Grid::GridManager projectGrids;
        TEST_CHECK(pm.openProject(projectPath, project, &projectGrids, &error) && pm.projectName() == QStringLiteral("Nouveau"),
                   "Test 474: projet ouvert sous son propre nom");
        const auto& d = projectGrids.activeGrid()->definition();
        TEST_CHECK((d.xLabels() == std::vector<std::string>{ "A", "D", "B", "C", "E" }) && approxEqual(d.xPositions()[1], 3.2)
                       && (project.levelManager()->elevationList() == std::vector<double>{ 0.0, 3.2, 6.4 }) && project.nodes().size() == 2,
                   "Test 474: grille irrégulière, niveaux et contenu restaurés");
        TEST_CHECK(hasPlane(project.detectStructuralPlanes(WorkPlaneAxis::X), 3.2, "D (X = 3.20 m)"), "Test 474: plans dérivés recalculés à l'ouverture");

        project.addNode(2.0, 2.0, 0.0);
        TEST_CHECK(pm.saveProject(projectPath, project, &projectGrids, QImage(), &error) && fileHash(info.path) == templateHash,
                   "Test 474: le projet modifié et enregistré ne modifie pas le template");
        TEST_CHECK(!store.createProject(info, projectPath, QStringLiteral("Nouveau"), &error) && error.contains(QStringLiteral("existe déjà")),
                   "Test 474: un fichier de projet existant n'est jamais écrasé");

        QFile corrupt(info.path);
        TEST_CHECK(corrupt.open(QIODevice::WriteOnly) && corrupt.write("ceci n'est pas un projet") > 0, "Test 474: préparation");
        corrupt.close();
        TSA::Model::Model target;
        TSA::Grid::GridManager targetGrids;
        target.addNode(0.0, 0.0, 0.0);
        TEST_CHECK(!store.load(info, target, &targetGrids, &error) && !error.isEmpty() && target.nodes().size() == 1,
                   "Test 474: template corrompu refusé, projet cible inchangé");
        std::cout << "[PASS] Test 474: Nouveau projet depuis un template : copie indépendante, aucun écrasement, template corrompu refusé" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 475 : compatibilité
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString dir = tmp.filePath(QStringLiteral("templates"));
        QDir().mkpath(dir);
        TSA::Model::Model model;
        TSA::Grid::GridManager grids;
        buildProject(model, grids);
        QString error;
        TEST_CHECK(TSA::IO::TSAProjectIO::saveProject(QDir(dir).filePath(QStringLiteral("Ancien gabarit.tsa")), model, &grids, QString(), QString(), true, QImage(), &error),
                   "Test 475: préparation");
        QStringList problems;
        const auto list = ProjectTemplateStore(dir).list(&problems);
        TEST_CHECK(list.size() == 1 && list.front().name == QStringLiteral("Ancien gabarit") && problems.size() == 1,
                   "Test 475: template sans fiche listé sous le nom du fichier (signalé)");

        // Ancien projet : niveaux fixes enregistrés par les versions précédentes
        model.levelManager()->setFromElevations({ 0.0, 3.0, 6.5, 10.0 }, { "RDC (Z=0.00)", "Étage 1 (Z=3.00)", "Étage 2 (Z=6.50)", "Toiture (Z=10.00)" });
        const QString old = tmp.filePath(QStringLiteral("ancien.tsa"));
        TEST_CHECK(TSA::IO::TSAProjectIO::saveProject(old, model, &grids, QString(), QString(), true, QImage(), &error), "Test 475: préparation");
        TSA::Project::ProjectManager pm;
        TSA::Model::Model reopened;
        TSA::Grid::GridManager reopenedGrids;
        TEST_CHECK(pm.openProject(old, reopened, &reopenedGrids, &error)
                       && (reopened.levelManager()->elevationList() == std::vector<double>{ 0.0, 3.2, 6.4 }) && reopened.nodes().size() == 2,
                   "Test 475: ancien .tsa ouvert, niveaux fixes remplacés par ceux de sa grille, contenu intact");
        std::cout << "[PASS] Test 475: Compatibilité : template sans fiche, anciens projets à niveaux fixes" << std::endl;
        ++passed;
    }
    return true;
}
