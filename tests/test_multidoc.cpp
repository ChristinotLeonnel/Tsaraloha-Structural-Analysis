// Suite « multidoc » : copier-coller entre projets (fusion des nœuds, conflits d'appuis, éléments dégénérés ou en
// double, fondations, cotations, combinaisons, annulation), erreurs d'ouverture de fichier.
//   379  fusion des nœuds : portique collé sur lui-même (nœuds séparés / fusionnés, doublons ignorés ou conservés),
//        poutre reliée à un nœud existant (connectivité), modèles indépendants (aucune fusion à distance), tolérance ;
//   380  conflits d'appuis : décision demandée sans rien modifier ; ne pas fusionner, conserver, remplacer ;
//        appui + nœud libre compatibles ;
//   381  élément dégénéré (deux nœuds fusionnés en un) : collage refusé, modèle et historique inchangés ;
//   382  fondations et cotations : propriétés, nœud porteur, ancrages reliés aux nœuds collés ou figés ;
//   383  combinaisons : pertinentes seulement, facteurs reportés, cas emportés, combinaison identique réutilisée,
//        homonymes renommés (cas et combinaisons), référence absente → non copiée avec avertissement ;
//   384  annulation / rétablissement d'un collage avec fusion (appui remplacé, charges, nœuds réutilisés),
//        identifiants sans collision, sections, matériaux, orientations, résultats non transférés ;
//   385  ouverture d'un fichier : absent → « introuvable », verrouillé par un autre processus → « inaccessible » ;
//   386  serveur MCP et plusieurs documents : un serveur par document, un seul à l'écoute (nom fixe) ; changement
//        d'onglet → connexions du document quitté fermées, reconnexion au document actif ; identifiant de document
//        dans chaque réponse ; document_id d'un autre document → erreur explicite, rien n'est exécuté ; document
//        devenu inactif → requête refusée ; list_documents ;
//   387  profils d'affichage : profils fournis, application sur une vraie vue (aucune donnée physique ni résultat
//        modifié), clés inconnues ignorées, clés absentes sans effet, valeurs hors bornes refusées, liste blanche à
//        l'enregistrement, créer / renommer / supprimer / rétablir, association à un document, persistance relue par un
//        autre processus (registre) ; réglages de l'utilisateur jamais touchés (identité de réglages isolée, effacée) ;
//   388  menus d'affichage partagés TSA / TSALab (UI/Display/DisplayMenus) : menus et actions, profil appliqué par le
//        composant (signal, effet sur la vue, association au document), refus sans vue, échelle des charges,
//        diagnostic ; aucune donnée du modèle modifiée.

#include "test_common.h"

#include "IO/TSAFile.h"
#include "Model/Load/LoadManager.h"
#include "Model/StructuralClipboard.h"
#include "Automation/AutomationServer.h"
#include "Project/ProjectSession.h"
#include "Viewer/DisplayProfile.h"
#include "Viewer/OccView.h"
#include "Viewer/ResultsVisualManager.h"
#include "Viewer/AnnotationStyle.h"
#include "UI/Display/DisplayMenus.h"
#include "Viewer/AxisDisplayStyle.h"
#include "Geometry/LoadDisplayScale.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QThread>
#include <QProcess>
#include <QSettings>
#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace TSA::Model;

namespace
{
struct Portal
{
    int a = 0, b = 0, c = 0, d = 0;   // pieds a, d ; têtes b, c
    int colAB = 0, beam = 0, colDC = 0;
};

/// Portique 6 m × 3 m : poteaux encastrés en pied, poutre IPE 300 orientée à 15°.
Portal portal(Model& m, double x0 = 0.0)
{
    Portal p;
    p.a = m.addNode(x0, 0, 0);
    p.b = m.addNode(x0, 0, 3);
    p.c = m.addNode(x0 + 6, 0, 3);
    p.d = m.addNode(x0 + 6, 0, 0);
    m.getNode(p.a)->setSupport(SupportDefinition::fixed());
    m.getNode(p.d)->setSupport(SupportDefinition::fixed());
    p.colAB = m.addColumn(p.a, p.b, Section::heb(200), Material::steelS235());
    p.beam = m.addBar(p.b, p.c, Section::ipe(300), Material::steelS355(), BarRole::Beam, 15.0);
    p.colDC = m.addColumn(p.d, p.c, Section::heb(200), Material::steelS235());
    return p;
}

ClipboardSelection portalSelection(const Portal& p)
{
    ClipboardSelection s;
    s.columns = { p.colAB, p.colDC };
    s.beams = { p.beam };
    return s;
}

PasteOptions merging(double tol = 1e-3)
{
    PasteOptions o;
    o.merge = NodeMergeMode::MergeCoincident;
    o.tolerance = tol;
    return o;
}

std::size_t elementCount(const Model& m)
{
    return m.beams().size() + m.columns().size() + m.slabs().size() + m.walls().size() + m.trussMembers().size() + m.cables().size() + m.foundations().size();
}
} // namespace

bool runSuite_MultiDoc(int& passed)
{
    auto& clip = StructuralClipboard::shared();

    // -------------------------------------------------------------------------
    // TEST 379 : fusion des nœuds coïncidents
    // -------------------------------------------------------------------------
    {
        // Portique collé sur lui-même, position d'origine
        Model m;
        const Portal p = portal(m);
        clip.copyFrom(m, portalSelection(p));
        const std::size_t n0 = m.nodes().size(), e0 = elementCount(m);

        const auto sep = clip.pasteUndoable(m, 0, 0, 0, "Coller");   // nœuds séparés (défaut)
        TEST_CHECK(sep.complete && m.nodes().size() == n0 + 4 && elementCount(m) == e0 + 3 && sep.mergedNodeIds.empty(),
                   "Test 379: sans fusion — nœuds coïncidents séparés, modèles indépendants (comportement historique)");
        m.undo();

        const auto plan = clip.analyzePaste(m, 0, 0, 0, merging());
        TEST_CHECK(plan.merges.size() == 4 && plan.duplicates.size() == 3 && plan.supportConflicts.empty() && !plan.blocked(),
                   "Test 379: analyse — 4 nœuds fusionnables, 3 éléments en double, appuis identiques (aucun conflit)");
        TEST_CHECK(m.nodes().size() == n0 && elementCount(m) == e0, "Test 379: l'analyse ne modifie rien");

        const auto skip = clip.pasteUndoable(m, 0, 0, 0, "Coller", merging());
        TEST_CHECK(skip.complete && m.nodes().size() == n0 && elementCount(m) == e0 && skip.skippedDuplicates == 3 && skip.mergedNodeIds.size() == 4,
                   "Test 379: fusion + doublons ignorés — aucun nœud ni élément ajouté (" << m.nodes().size() << " nœuds, " << elementCount(m) << " éléments)");
        m.undo();
        auto keepOpt = merging();
        keepOpt.duplicates = DuplicateElementPolicy::Keep;
        const auto keep = clip.pasteUndoable(m, 0, 0, 0, "Coller", keepOpt);
        TEST_CHECK(keep.complete && m.nodes().size() == n0 && elementCount(m) == e0 + 3 && !keep.warnings.empty(),
                   "Test 379: fusion + doublons conservés — éléments ajoutés sur les nœuds existants, avertissement");
        m.undo();

        // Poutre seule reliée à un nœud existant : un seul nœud fusionné, connectivité assurée
        Model t;
        const Portal tp = portal(t);
        Model src;
        const int s1 = src.addNode(0, 0, 0), s2 = src.addNode(4, 0, 0);
        const int sb = src.addBar(s1, s2, Section::ipe(200), Material::steelS235(), BarRole::Beam);
        ClipboardSelection sel;
        sel.beams = { sb };
        clip.copyFrom(src, sel);
        const std::size_t tn = t.nodes().size();
        const auto r = clip.pasteUndoable(t, 6, 0, 3, "Coller", merging());   // départ sur la tête c du portique
        const Beam* nb = r.beamIds.empty() ? nullptr : t.getBeam(r.beamIds[0]);
        TEST_CHECK(r.complete && r.mergedNodeIds == std::vector<int> { tp.c } && t.nodes().size() == tn + 1 && nb && nb->startNodeId() == tp.c,
                   "Test 379: poutre collée sur un nœud existant — nœud réutilisé, poutre reliée au portique");

        // Tolérance : nœud à 0,6 mm fusionné (tolérance 1 mm), à 2 mm non fusionné ; jamais à distance
        Model t2;
        const Portal tp2 = portal(t2);
        (void)tp2;
        const auto nearPlan = clip.analyzePaste(t2, 6.0006, 0, 3, merging(1e-3));
        const auto farPlan = clip.analyzePaste(t2, 6.002, 0, 3, merging(1e-3));
        const auto away = clip.analyzePaste(t2, 50, 50, 0, merging(1e-3));
        TEST_CHECK(nearPlan.merges.size() == 1 && farPlan.merges.empty() && away.merges.empty(), "Test 379: distance réelle comparée à la tolérance (0,6 mm fusionné, 2 mm et 50 m non)");
        TEST_CHECK(clip.analyzePaste(t2, 6, 0, 3, merging(0.0)).blocked(), "Test 379: tolérance nulle refusée");
        std::cout << "[PASS] Test 379: Fusion des nœuds coïncidents (portique sur lui-même, poutre reliée, modèles indépendants)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 380 : conflits de conditions aux limites
    // -------------------------------------------------------------------------
    {
        Model m;
        const Portal p = portal(m);
        m.getNode(p.d)->setSupport(SupportDefinition::pinned());   // pied d articulé dans le projet cible
        Model src;
        const Portal sp = portal(src);   // pieds encastrés dans le presse-papier
        clip.copyFrom(src, portalSelection(sp));
        const std::size_t n0 = m.nodes().size(), e0 = elementCount(m);
        const bool couldUndo = m.canUndo();
        auto opt = merging();
        opt.duplicates = DuplicateElementPolicy::Keep;
        const auto ask = clip.pasteUndoable(m, 0, 0, 0, "Coller", opt);
        TEST_CHECK(!ask.complete && ask.needsDecision && ask.conflicts.size() == 1 && ask.conflicts[0].targetNodeId == p.d,
                   "Test 380: appuis incompatibles (encastrement copié / articulation existante) — décision demandée (" << ask.error << ")");
        TEST_CHECK(m.nodes().size() == n0 && elementCount(m) == e0 && m.getNode(p.d)->support().isPinned() && m.canUndo() == couldUndo,
                   "Test 380: rien n'est modifié tant que la décision n'est pas prise (ni modèle ni historique)");

        opt.supportConflicts = SupportConflictPolicy::DontMerge;
        const auto sep = clip.pasteUndoable(m, 0, 0, 0, "Coller", opt);
        TEST_CHECK(sep.complete && sep.mergedNodeIds.size() == 3 && m.nodes().size() == n0 + 1 && m.getNode(p.d)->support().isPinned(),
                   "Test 380: « ne pas fusionner » — 3 nœuds fusionnés, le pied en conflit reste un nœud séparé");
        m.undo();
        opt.supportConflicts = SupportConflictPolicy::KeepTarget;
        const auto kt = clip.pasteUndoable(m, 0, 0, 0, "Coller", opt);
        TEST_CHECK(kt.complete && kt.mergedNodeIds.size() == 4 && m.getNode(p.d)->support().isPinned(), "Test 380: « conserver » — appui du projet conservé");
        m.undo();
        opt.supportConflicts = SupportConflictPolicy::UseCopied;
        const auto uc = clip.pasteUndoable(m, 0, 0, 0, "Coller", opt);
        TEST_CHECK(uc.complete && m.getNode(p.d)->support().isFixed(), "Test 380: « utiliser les appuis copiés » — encastrement appliqué");
        m.undo();
        TEST_CHECK(m.getNode(p.d)->support().isPinned(), "Test 380: annulation — articulation d'origine rétablie");

        // Appui copié sur un nœud existant libre : compatible, l'appui est ajouté (aucune propriété perdue)
        Model t;
        const int f1 = t.addNode(0, 0, 0), f2 = t.addNode(0, 0, 3);
        (void)f2;
        const auto ok = clip.pasteUndoable(t, 0, 0, 0, "Coller", merging());
        TEST_CHECK(ok.complete && t.getNode(f1)->support().isFixed(), "Test 380: nœud existant libre + appui copié — compatible, appui conservé");
        std::cout << "[PASS] Test 380: Conflits de conditions aux limites (décision, séparer, conserver, remplacer)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 381 : élément dégénéré
    // -------------------------------------------------------------------------
    {
        Model src;
        const int s1 = src.addNode(0, 0, 0), s2 = src.addNode(0.4, 0, 0);
        const int sb = src.addBar(s1, s2, Section::ipe(200), Material::steelS235(), BarRole::Beam);
        ClipboardSelection sel;
        sel.beams = { sb };
        clip.copyFrom(src, sel);
        Model t;
        const int n = t.addNode(10.2, 0, 0);
        (void)n;
        const std::size_t n0 = t.nodes().size(), e0 = elementCount(t);
        const bool couldUndo = t.canUndo();
        // Tolérance 0,5 m : les deux extrémités (10,0 et 10,4) tombent sur le même nœud existant (10,2)
        const auto r = clip.pasteUndoable(t, 10.0, 0, 0, "Coller", merging(0.5));
        TEST_CHECK(!r.complete && !r.needsDecision && !r.conflicts.empty() && r.conflicts[0].kind == PasteConflict::Kind::Degenerate,
                   "Test 381: poutre de longueur nulle après fusion — collage refusé (" << r.error << ")");
        TEST_CHECK(t.nodes().size() == n0 && elementCount(t) == e0 && t.canUndo() == couldUndo, "Test 381: modèle et historique inchangés");
        std::cout << "[PASS] Test 381: Élément dégénéré refusé sans modification" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 382 : fondations et cotations
    // -------------------------------------------------------------------------
    {
        Model src;
        const Portal p = portal(src);
        const int f = src.addFoundation(p.a, 1.8, 2.2, 0.6, "S1");
        src.getFoundation(f)->setMaterial(Material::concreteC30_37());
        src.getFoundation(f)->setSoilBearingCapacity(320.0);
        // Cotation linéaire : a (copié) → point d'un nœud non copié ; cotation de niveau sur b
        const int farNode = src.addNode(20, 0, 0);
        TSA::Annotation::Dimension lin;
        lin.kind = TSA::Annotation::DimensionKind::Linear;
        lin.anchors = { { p.a, { 0, 0, 0 } }, { farNode, { 20, 0, 0 } } };
        lin.position = { 10, -1, 0 };
        TSA::Annotation::Dimension lvl;
        lvl.kind = TSA::Annotation::DimensionKind::Level;
        lvl.anchors = { { p.b, { 0, 0, 3 } } };
        lvl.position = { -1, 0, 3 };
        auto& ds = src.dimensionsForEdit();
        lin.id = ds.nextId++;
        ds.items[lin.id] = lin;
        lvl.id = ds.nextId++;
        ds.items[lvl.id] = lvl;
        ClipboardSelection sel = portalSelection(p);
        sel.foundations = { f };
        sel.dimensions = { lin.id, lvl.id };
        clip.copyFrom(src, sel);
        TEST_CHECK(clip.foundationCount() == 1 && clip.dimensionCount() == 2, "Test 382: fondation et 2 cotations copiées");

        Model t;
        const auto r = clip.pasteUndoable(t, 100, 0, 0, "Coller");
        const Foundation* pf = r.foundationIds.empty() ? nullptr : t.getFoundation(r.foundationIds[0]);
        const Node* pfn = pf ? t.getNode(pf->nodeId()) : nullptr;
        TEST_CHECK(r.complete && pf && pf->widthA() == 1.8 && pf->lengthB() == 2.2 && pf->heightH() == 0.6 && pf->soilBearingCapacity() == 320.0 &&
                       pf->material().name == Material::concreteC30_37().name && pfn && pfn->x() == 100.0 && pfn->z() == 0.0,
                   "Test 382: fondation collée (dimensions, matériau, portance) sur le pied collé");
        TEST_CHECK(r.dimensionIds.size() == 2 && t.dimensions().items.size() == 2, "Test 382: 2 cotations collées");
        const auto& plin = t.dimensions().items.at(r.dimensionIds[0]);
        const auto& plvl = t.dimensions().items.at(r.dimensionIds[1]);
        const int pa = r.nodeMap.at(p.a), pb = r.nodeMap.at(p.b);
        TEST_CHECK(plin.anchors[0].nodeId == pa && plin.anchors[1].nodeId == -1 && plin.anchors[1].point[0] == 120.0 && plin.position[0] == 110.0 &&
                       !plin.hasInvalidReference(),
                   "Test 382: ancrage sur un nœud copié relié au nœud collé ; ancrage sur un nœud non copié figé et translaté");
        TEST_CHECK(plvl.anchors[0].nodeId == pb && plvl.anchors[0].point[2] == 3.0, "Test 382: cotation de niveau reliée au nœud collé");
        TEST_CHECK(t.undo() && t.foundations().empty() && t.dimensions().items.empty(), "Test 382: annulation — fondation et cotations retirées");
        std::cout << "[PASS] Test 382: Fondations et cotations copiées" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 383 : combinaisons de charges
    // -------------------------------------------------------------------------
    {
        Model src;
        const Portal p = portal(src);
        auto& lm = src.loadManager();
        lm.clear();
        const int g = lm.addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
        const int q = lm.addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int w = lm.addLoadCase(LoadCase(0, "W", LoadCaseCategory::Wind));
        const int s = lm.addLoadCase(LoadCase(0, "S", LoadCaseCategory::Snow));
        lm.addMemberLoad(MemberLoad::uniform(p.beam, g, 5.0));
        lm.addMemberLoad(MemberLoad::uniform(p.beam, q, 3.0));
        const int elu = lm.addCombination(LoadCombination(0, "ELU 1", LoadCombinationType::ULS_Fundamental, { { g, 1.35 }, { q, 1.5 } }));
        const int vent = lm.addCombination(LoadCombination(0, "ELU vent", LoadCombinationType::ULS_Fundamental, { { g, 1.0 }, { w, 1.5 } }));
        const int neige = lm.addCombination(LoadCombination(0, "ELS neige", LoadCombinationType::SLS_Characteristic, { { s, 1.0 } }));
        const int dangling = lm.addCombination(LoadCombination(0, "Ancienne", LoadCombinationType::ULS_Fundamental, { { g, 1.0 }, { 999, 1.0 } }));
        (void)elu;
        (void)vent;
        (void)neige;
        (void)dangling;
        ClipboardSelection sel = portalSelection(p);
        ClipboardOptions opt;
        opt.loads = true;
        clip.copyFrom(src, sel, opt);
        TEST_CHECK(clip.combinationCount() == 2 && clip.copyWarnings().size() == 1 && clip.copyWarnings()[0].find("Ancienne") != std::string::npos,
                   "Test 383: combinaisons pertinentes copiées (ELU 1, ELU vent), sans rapport ignorée (ELS neige), référence absente signalée (Ancienne)");

        // Projet cible : « G » identique, « Q » de catégorie différente (homonyme), « ELU 1 » identique déjà présente,
        // « ELU vent » homonyme de contenu différent
        Model t;
        auto& tl = t.loadManager();
        tl.clear();
        const int tg = tl.addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
        const int tq = tl.addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Snow));
        (void)tq;
        const std::size_t combos0 = tl.combinations().size();
        const auto r = clip.pasteUndoable(t, 50, 0, 0, "Coller");
        TEST_CHECK(r.complete, "Test 383: collage complet (" << r.error << ")");
        int newQ = 0, newW = 0;
        for (const auto& [id, lc] : tl.loadCases())
        {
            if (lc.name() == "Q (copie)" && lc.category() == LoadCaseCategory::Live) newQ = id;
            if (lc.name() == "W") newW = id;
        }
        TEST_CHECK(r.loadCaseMap.at(g) == tg && newQ != 0 && newW != 0, "Test 383: G rapproché, Q homonyme d'autre catégorie créé sous « Q (copie) », W emporté pour « ELU vent »");
        const LoadCombination* c1 = nullptr;
        const LoadCombination* c2 = nullptr;
        for (const auto& [id, c] : tl.combinations())
        {
            if (c.name() == "ELU 1") c1 = &c;
            if (c.name() == "ELU vent") c2 = &c;
        }
        TEST_CHECK(c1 && c1->factor(tg) == 1.35 && c1->factor(newQ) == 1.5 && c2 && c2->factor(tg) == 1.0 && c2->factor(newW) == 1.5 &&
                       tl.combinations().size() == combos0 + 2,
                   "Test 383: facteurs reportés sur les cas cibles");
        // Second collage : combinaisons identiques réutilisées, pas de doublon
        const std::size_t combos1 = tl.combinations().size();
        const auto r2 = clip.pasteUndoable(t, 80, 0, 0, "Coller");
        TEST_CHECK(r2.complete && r2.combinationIds.empty() && tl.combinations().size() == combos1, "Test 383: combinaison identique déjà présente réutilisée");
        // Homonyme de contenu différent : renommée
        int elu1 = 0;
        for (const auto& [id, c] : tl.combinations())
            if (c.name() == "ELU 1") elu1 = id;
        if (auto* c = tl.getCombination(elu1)) c->setFactor(tg, 1.0);
        const auto r3 = clip.pasteUndoable(t, 110, 0, 0, "Coller");
        bool renamed = false;
        for (const auto& [id, c] : tl.combinations()) renamed = renamed || c.name() == "ELU 1 (copie)";
        TEST_CHECK(r3.complete && renamed && !r3.warnings.empty(), "Test 383: homonyme de contenu différent — copiée sous « ELU 1 (copie) », avertissement");
        std::cout << "[PASS] Test 383: Combinaisons de charges (pertinence, facteurs, homonymes, références absentes)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 384 : annulation / rétablissement, identifiants, propriétés
    // -------------------------------------------------------------------------
    {
        Model src;
        const Portal p = portal(src);
        auto& lm = src.loadManager();
        lm.clear();
        const int g = lm.addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
        lm.addNodalLoad(NodalLoad(0, p.b, g, 0, 0, -40.0));
        lm.addMemberLoad(MemberLoad::uniform(p.beam, g, 8.0));
        ClipboardOptions opt;
        opt.loads = true;
        clip.copyFrom(src, portalSelection(p), opt);

        Model t;
        const Portal tp = portal(t, 6.0);   // portique voisin : son pied a (6, 0, 0) coïncide avec le pied d copié
        t.getNode(tp.a)->setSupport(SupportDefinition::free());
        const int maxNode0 = t.nodes().rbegin()->first, maxBeam0 = t.beams().rbegin()->first, maxCol0 = t.columns().rbegin()->first;
        const std::size_t n0 = t.nodes().size(), e0 = elementCount(t), l0 = t.loadManager().nodalLoads().size() + t.loadManager().memberLoads().size();
        auto po = merging();
        const auto r = clip.pasteUndoable(t, 0, 0, 0, "Coller", po);
        // Le poteau copié d→c tombe sur le poteau existant a→b : doublon non collé (poteau commun aux deux portiques)
        TEST_CHECK(r.complete && r.mergedNodeIds.size() == 2 && t.nodes().size() == n0 + 2 && elementCount(t) == e0 + 2 && r.skippedDuplicates == 1 &&
                       t.getNode(tp.a)->support().isFixed(),
                   "Test 384: deux portiques accolés — 2 nœuds partagés, poteau commun non doublé, appui ajouté au nœud libre");
        bool freshIds = true;
        for (int id : r.beamIds) freshIds = freshIds && id > maxBeam0;
        for (int id : r.columnIds) freshIds = freshIds && id > maxCol0;
        for (int id : r.nodeIds) freshIds = freshIds && id > maxNode0;
        TEST_CHECK(freshIds, "Test 384: identifiants nouveaux, sans collision avec les existants");
        const Beam* nb = t.getBeam(r.beamIds.at(0));
        TEST_CHECK(nb && nb->properties().section.name == Section::ipe(300).name && nb->properties().material.name == Material::steelS355().name &&
                       nb->properties().rotation == 15.0,
                   "Test 384: section, matériau et orientation locale conservés");
        TEST_CHECK(t.loadManager().nodalLoads().size() + t.loadManager().memberLoads().size() == l0 + 2, "Test 384: charges collées");
        TEST_CHECK(t.undo() && t.nodes().size() == n0 && elementCount(t) == e0 && !t.getNode(tp.a)->support().isSupported() &&
                       t.loadManager().nodalLoads().size() + t.loadManager().memberLoads().size() == l0,
                   "Test 384: Annuler — nœuds, éléments, charges et appui modifié rétablis en une étape");
        TEST_CHECK(t.redo() && t.nodes().size() == n0 + 2 && t.getNode(tp.a)->support().isFixed(), "Test 384: Rétablir — collage fusionné rétabli");
        TEST_CHECK(t.analysisSettingsJson().empty(), "Test 384: aucun résultat ni réglage de calcul transféré par le presse-papier");
        std::cout << "[PASS] Test 384: Annuler / Rétablir, identifiants, sections, matériaux, orientations" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 385 : messages d'erreur à l'ouverture d'un projet
    // -------------------------------------------------------------------------
    {
        QTemporaryDir dir;
        const std::string missing = dir.filePath("absent.tsa").toStdString();
        TSA::IO::TSAFileHeader header;
        std::string err;
        TEST_CHECK(!TSA::IO::TSAFileReader::readHeader(missing, header, &err) && err.find("introuvable") != std::string::npos, "Test 385: fichier absent — « introuvable » (" << err << ")");
#ifdef Q_OS_WIN
        const QString path = dir.filePath("verrou.tsa");
        {
            QFile f(path);
            f.open(QIODevice::WriteOnly);
            f.write("TSA");
        }
        HANDLE h = CreateFileW(QDir::toNativeSeparators(path).toStdWString().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        err.clear();
        const bool read = TSA::IO::TSAFileReader::readHeader(path.toStdString(), header, &err);
        CloseHandle(h);
        TEST_CHECK(!read && err.find("inaccessible") != std::string::npos && err.find("introuvable") == std::string::npos,
                   "Test 385: fichier verrouillé par un autre processus — « inaccessible », pas « introuvable » (" << err << ")");
#endif
        std::cout << "[PASS] Test 385: Messages d'erreur d'ouverture (absent, verrouillé)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 386 : serveur MCP avec plusieurs documents
    // -------------------------------------------------------------------------
    {
        using TSA::Automation::AutomationServer;
        TSA::Project::ProjectSession docA, docB;
        AutomationServer srvA(&docA), srvB(&docB);
        srvA.setDocumentId("doc-A");
        srvB.setDocumentId("doc-B");
        bool aActive = true, bActive = false;
        srvA.setActiveCheck([&] { return aActive; });
        srvB.setActiveCheck([&] { return bActive; });
        auto docs = [&] {
            return QJsonArray { QJsonObject { { "id", "doc-A" }, { "name", "A.tsa" }, { "active", aActive } },
                                QJsonObject { { "id", "doc-B" }, { "name", "B.tsa" }, { "active", bActive } } };
        };
        srvA.setDocumentsProvider(docs);
        srvB.setDocumentsProvider(docs);
        const QString name = QStringLiteral("tsaraloha-multidoc-%1").arg(QCoreApplication::applicationPid());
        QString err;
        TEST_CHECK(srvA.start(name, &err), "Test 386: serveur du document A à l'écoute (" << err.toStdString() << ")");
        TEST_CHECK(!srvB.start(name, &err), "Test 386: un seul serveur peut écouter sous le nom fixe (B refusé tant que A écoute)");

        auto pump = [](int ms) {
            QElapsedTimer t;
            t.start();
            while (t.elapsed() < ms)
            {
                QCoreApplication::processEvents();
                QThread::msleep(5);
            }
        };
        // Requête par une vraie connexion locale ; réponse attendue sans bloquer le serveur (même thread)
        auto request = [&](QLocalSocket& sock, const QString& method, const QJsonObject& params) {
            sock.write(QJsonDocument(QJsonObject { { "id", 1 }, { "method", method }, { "params", params } }).toJson(QJsonDocument::Compact) + '\n');
            sock.flush();
            QElapsedTimer t;
            t.start();
            while (!sock.canReadLine() && t.elapsed() < 3000 && sock.state() == QLocalSocket::ConnectedState)
            {
                QCoreApplication::processEvents();
                sock.waitForReadyRead(10);
            }
            return sock.canReadLine() ? QJsonDocument::fromJson(sock.readLine()).object() : QJsonObject {};
        };
        QLocalSocket client;
        client.connectToServer(name);
        pump(50);
        TEST_CHECK(client.waitForConnected(1000) || client.state() == QLocalSocket::ConnectedState, "Test 386: client connecté");
        pump(50);
        const auto h1 = request(client, "hello", {});
        TEST_CHECK(h1.value("document").toObject().value("id").toString() == "doc-A", "Test 386: réponse étiquetée par le document servi (A)");
        const auto ld = request(client, "list_documents", {});
        const auto list = ld.value("result").toObject().value("documents").toArray();
        TEST_CHECK(list.size() == 2 && ld.value("result").toObject().value("active").toString() == "doc-A", "Test 386: list_documents — 2 documents, A actif");

        // document_id d'un autre document : erreur explicite, rien n'est exécuté
        const std::size_t nA = docA.model().nodes().size();
        const auto wrong = request(client, "execute", { { "document_id", "doc-B" }, { "command", "model.create_node" },
                                                        { "arguments", QJsonObject { { "position", QJsonArray { 0, 0, 0 } } } } });
        TEST_CHECK(wrong.contains("error") && docA.model().nodes().size() == nA && docB.model().nodes().empty(),
                   "Test 386: document_id ≠ document actif — erreur explicite, aucun document modifié");
        const auto right = request(client, "execute", { { "document_id", "doc-A" }, { "command", "model.create_node" },
                                                        { "arguments", QJsonObject { { "position", QJsonArray { 0, 0, 0 } } } } });
        TEST_CHECK(right.value("result").toObject().value("ok").toBool() && docA.model().nodes().size() == nA + 1, "Test 386: document_id du document actif — exécuté");

        // Document devenu inactif sans arrêt du serveur (garde de dernier recours) : requête refusée
        aActive = false;
        const auto stale = request(client, "execute", { { "command", "model.create_node" }, { "arguments", QJsonObject { { "position", QJsonArray { 1, 0, 0 } } } } });
        TEST_CHECK(stale.contains("error") && docA.model().nodes().size() == nA + 1, "Test 386: document plus actif — requête refusée");

        // Changement d'onglet (AppShell::activateDocument) : A arrêté → sa connexion fermée ; B à l'écoute
        srvA.stop();
        bActive = true;
        TEST_CHECK(srvB.start(name, &err), "Test 386: serveur du document B à l'écoute après l'arrêt de A (" << err.toStdString() << ")");
        pump(100);
        if (client.state() != QLocalSocket::UnconnectedState) client.waitForDisconnected(1000);
        TEST_CHECK(client.state() == QLocalSocket::UnconnectedState, "Test 386: connexion au document quitté fermée (aucune référence obsolète)");
        QLocalSocket client2;
        client2.connectToServer(name);
        pump(50);
        client2.waitForConnected(1000);
        pump(50);
        const auto h2 = request(client2, "hello", {});
        const auto onB = request(client2, "execute", { { "command", "model.create_node" }, { "arguments", QJsonObject { { "position", QJsonArray { 2, 0, 0 } } } } });
        TEST_CHECK(h2.value("document").toObject().value("id").toString() == "doc-B" && onB.value("result").toObject().value("ok").toBool() &&
                       docB.model().nodes().size() == 1 && docA.model().nodes().size() == nA + 1,
                   "Test 386: reconnexion — commandes exécutées sur le document actif B, A intact");
        client2.disconnectFromServer();
        srvB.stop();
        std::cout << "[PASS] Test 386: Serveur MCP et plusieurs documents (document actif, identifiant, refus explicite)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 387 : profils d'affichage
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Viewer;
        // Identité de réglages isolée (la suite n'en a pas : QSettings n'écrirait rien) ; effacée en fin de test
        const QString savedOrg = QCoreApplication::organizationName(), savedApp = QCoreApplication::applicationName();
        const QString isoOrg = QStringLiteral("TSA_TestSuite_Isolated"), isoApp = QStringLiteral("profiles%1").arg(QCoreApplication::applicationPid());
        QCoreApplication::setOrganizationName(isoOrg);
        QCoreApplication::setApplicationName(isoApp);
        auto& style = AnnotationStyle::instance();
        const AnnotationSettings savedStyle = style.settings();

        DisplayProfileStore store;
        const auto defaults = store.profiles();
        QStringList names;
        for (const auto& p : defaults) names << p.name;
        TEST_CHECK(names == QStringList({ "Modélisation", "Charges", "Maillage", "Résultats", "Déformées", "Analyse modale", "Présentation" }),
                   "Test 387: 7 profils fournis au premier usage (" << names.join(", ").toStdString() << ")");

        // Vraie vue, modèle chargé et calculé
        Model m;
        const Portal pt = portal(m);
        auto& lm = m.loadManager();
        const int lc = lm.addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        lm.addMemberLoad(MemberLoad::uniform(pt.beam, lc, 12.0));
        OccView view;
        view.resize(640, 480);
        view.show();
        for (int k = 0; k < 20; ++k) QApplication::processEvents(), QThread::msleep(5);
        view.setModel(&m);
        view.rebuildAllShapes();
        const auto revision0 = m.revision();
        const double q0 = lm.memberLoads().begin()->second.q1();

        TEST_CHECK(applyDisplaySettings(view, store.profile("Présentation").settings).isEmpty() && !view.isGridVisible() && !view.areNodesVisible() &&
                       !view.areSupportsVisible() && !view.areLoadsVisible(),
                   "Test 387: « Présentation » appliqué — grille, nœuds, appuis, charges masqués");
        applyDisplaySettings(view, store.profile("Charges").settings);
        TEST_CHECK(view.areLoadsVisible() && view.areLoadValuesVisible() && view.modelDisplayMode() == ModelDisplayMode::Analytical,
                   "Test 387: « Charges » appliqué — charges et valeurs visibles, représentation filaire");
        TEST_CHECK(m.revision() == revision0 && lm.memberLoads().begin()->second.q1() == q0 && m.nodes().size() == 4,
                   "Test 387: aucune donnée physique modifiée (révision du modèle, charges, nœuds)");

        // Capture → application : état identique
        const QJsonObject snap = captureDisplaySettings(view);
        applyDisplaySettings(view, store.profile("Modélisation").settings);
        applyDisplaySettings(view, snap);
        TEST_CHECK(captureDisplaySettings(view) == snap, "Test 387: capture puis application — état d'affichage identique");

        // Clés inconnues ignorées, clés absentes sans effet, valeurs hors bornes refusées
        const double ls0 = view.loadScale();
        const QStringList unknown = applyDisplaySettings(view, QJsonObject { { "futureOption", 3 }, { "section", "IPE 600" }, { "grid", true }, { "loadScale", 1e9 } });
        TEST_CHECK(unknown.contains("futureOption") && unknown.contains("section") && view.isGridVisible() && view.loadScale() == ls0 && view.areLoadsVisible(),
                   "Test 387: clés inconnues ignorées et signalées, absentes sans effet, valeur hors bornes refusée");

        // Créer (liste blanche), renommer, supprimer, rétablir
        QJsonObject mine = captureDisplaySettings(view);
        mine["section"] = "IPE 600";   // jamais enregistré
        QString err;
        TEST_CHECK(store.save({ "Mon profil", mine }, &err) && store.contains("Mon profil") && !store.profile("Mon profil").settings.contains("section"),
                   "Test 387: profil créé ; clé non graphique écartée à l'enregistrement");
        TEST_CHECK(!store.save({ "  ", mine }, &err), "Test 387: nom vide refusé");
        TEST_CHECK(!store.rename("Mon profil", "Charges", &err) && store.rename("Mon profil", "Vue client", &err) && store.contains("Vue client") && !store.contains("Mon profil"),
                   "Test 387: renommage (nom déjà pris refusé)");
        store.setDocumentProfile("C:/Projets/Pont.tsa", "Vue client");
#ifdef Q_OS_WIN
        TEST_CHECK(store.documentProfile("c:/projets/PONT.tsa") == "Vue client", "Test 387: profil associé au document (chemin normalisé)");
#else
        // Système de fichiers sensible à la casse (normalizedPath) : « PONT.tsa » est un autre fichier.
        TEST_CHECK(store.documentProfile("C:/Projets/Pont.tsa") == "Vue client" && store.documentProfile("c:/projets/PONT.tsa").isEmpty(),
                   "Test 387: profil associé au document (chemin exact, casse significative)");
#endif
        TEST_CHECK(store.remove("Résultats") && !store.contains("Résultats"), "Test 387: suppression");
        store.restoreDefaults();
        TEST_CHECK(store.contains("Résultats") && store.contains("Vue client"), "Test 387: profils fournis rétablis, profils de l'utilisateur conservés");

        // Persistance : relue par un autre processus (registre), puis par un nouvel objet (redémarrage)
        QSettings().sync();
#ifdef Q_OS_WIN
        QProcess reg;
        reg.start("reg", { "query", QStringLiteral("HKCU\\Software\\%1\\%2\\Display\\Profiles").arg(isoOrg, isoApp), "/v", "list" });
        reg.waitForFinished(10000);
        const QString regOut = QString::fromLocal8Bit(reg.readAllStandardOutput());
        TEST_CHECK(regOut.contains("Vue client"), "Test 387: profils enregistrés sur disque (registre lu par reg.exe)");
#else
        // Format natif hors Windows : fichier INI de QSettings, relu directement sur le disque.
        QFile ini(QSettings().fileName());
        const QString iniText = ini.open(QIODevice::ReadOnly) ? QString::fromUtf8(ini.readAll()) : QString();
        TEST_CHECK(iniText.contains("Vue client"), "Test 387: profils enregistrés sur disque (" << ini.fileName().toStdString() << ")");
#endif
        DisplayProfileStore again;
        TEST_CHECK(again.contains("Vue client") && again.documentProfile("C:/Projets/Pont.tsa") == "Vue client", "Test 387: profils relus par un nouvel objet (après redémarrage)");

        // Liste illisible (valeur corrompue) : chargement sans échec
        QSettings().setValue("Display/Profiles/list", "{pas du json");
        TEST_CHECK(DisplayProfileStore().profiles().isEmpty(), "Test 387: liste corrompue — chargée vide, sans échec");

        view.setModel(nullptr);
        view.hide();
        // Nettoyage : réglages isolés effacés, identité et style d'annotation d'origine rétablis
        {
            QSettings iso;
            iso.clear();
            iso.sync();
        }
        QProcess::execute("reg", { "delete", QStringLiteral("HKCU\\Software\\%1").arg(isoOrg), "/f" });
        style.setMode(savedStyle.mode);
        style.setBasePixels(savedStyle.basePixels);
        style.setModelHeight(savedStyle.modelHeight);
        QCoreApplication::setOrganizationName(savedOrg);
        QCoreApplication::setApplicationName(savedApp);
        std::cout << "[PASS] Test 387: Profils d'affichage (fournis, application, liste blanche, gestion, document, persistance)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 388 : menus d'affichage partagés (TSA et TSALab)
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Viewer;
        const QString savedOrg = QCoreApplication::organizationName(), savedApp = QCoreApplication::applicationName();
        const QString isoOrg = QStringLiteral("TSA_TestSuite_Isolated"), isoApp = QStringLiteral("menus%1").arg(QCoreApplication::applicationPid());
        QCoreApplication::setOrganizationName(isoOrg);
        QCoreApplication::setApplicationName(isoApp);
        const AnnotationSettings savedStyle = AnnotationStyle::instance().settings();

        Model m;
        const Portal pt = portal(m);
        (void)pt;
        QWidget host;
        OccView view(&host);
        host.resize(640, 480);
        host.show();
        view.resize(640, 480);
        for (int k = 0; k < 20; ++k) QApplication::processEvents(), QThread::msleep(5);
        view.setModel(&m);
        view.rebuildAllShapes();
        OccView* current = &view;
        QString docPath = QStringLiteral("C:/Projets/Essai.tsalab");
        TSA::UI::DisplayMenus menus(&host, { [&] { return current; }, [&]() -> const Model* { return &m; }, [&] { return docPath; } });
        auto titles = [](QMenu* menu) {
            QStringList t;
            for (QAction* a : menu->actions())
                if (!a->isSeparator()) t << a->text();
            return t;
        };
        TEST_CHECK(menus.annotationMenu() && titles(menus.annotationMenu()).contains("Taille constante à l'écran") &&
                       titles(menus.loadScaleMenu()).contains("Proportionnelle") && titles(menus.profilesMenu()).contains("Présentation") &&
                       menus.diagnosticAction(),
                   "Test 388: menus d'échelles, de profils et diagnostic construits par le composant partagé");
        QMenu viewMenu;
        menus.addTo(&viewMenu);
        TEST_CHECK(viewMenu.actions().size() == 5, "Test 388: ajout au menu Affichage de la fenêtre hôte (4 menus + diagnostic)");

        auto& axisStyle = TSA::Viewer::AxisDisplayStyle::instance();
        const TSA::Viewer::AxisDisplaySettings axisSettings = axisStyle.settings();
        axisStyle.reset();
        const QStringList axisTitles = titles(menus.axisMenu());
        TEST_CHECK(axisTitles.contains("Repère global") && axisTitles.contains("Indicateur d'angle") && axisTitles.contains("Lettres des axes") &&
                       axisTitles.contains("Repère local au point cliqué") && axisTitles.contains("Taille des axes…"),
                   "Test 388: menu Repères construit par le composant partagé");
        QAction* localAxes = nullptr;
        for (QAction* a : menus.axisMenu()->actions())
            if (a->text() == "Repères locaux") localAxes = a;
        TEST_CHECK(localAxes && localAxes->isChecked(), "Test 388: case des repères locaux conforme au réglage");
        localAxes->trigger();
        TEST_CHECK(!axisStyle.settings().localAxes, "Test 388: la case change le réglage");
        axisStyle.setLocalAxes(true);
        TEST_CHECK(localAxes->isChecked(), "Test 388: le réglage met à jour la case");
        axisStyle.setSettings(axisSettings);

        int applied = 0;
        QObject::connect(&menus, &TSA::UI::DisplayMenus::profileApplied, [&](const QString&) { ++applied; });
        const auto revision0 = m.revision();
        TEST_CHECK(menus.applyProfile("Présentation") && applied == 1 && !view.isGridVisible() && !view.areNodesVisible() && menus.activeProfile() == "Présentation",
                   "Test 388: profil appliqué par le composant — signal émis, vue modifiée");
        TEST_CHECK(DisplayProfileStore().documentProfile(docPath) == "Présentation", "Test 388: profil associé au document de la fenêtre hôte");
        TEST_CHECK(!menus.applyProfile("Inexistant") && applied == 1, "Test 388: profil inconnu refusé");
        // Échelle des charges : l'action « Normalisée » agit sur la vue du document
        for (QAction* a : menus.loadScaleMenu()->actions())
            if (a->text() == "Normalisée") a->trigger();
        TEST_CHECK(view.loadScaleSettings().mode == TSA::Geometry::LoadScaleMode::Normalized, "Test 388: échelle des charges réglée sur la vue du document");
        TEST_CHECK(menus.diagnosticText().contains("DIAGNOSTIC D'AFFICHAGE"), "Test 388: diagnostic de la vue");
        TEST_CHECK(m.revision() == revision0 && m.nodes().size() == 4, "Test 388: aucune donnée du modèle modifiée");
        current = nullptr;   // fenêtre sans vue (document fermé)
        TEST_CHECK(!menus.applyProfile("Modélisation") && menus.diagnosticText().isEmpty(), "Test 388: sans vue — refus, aucun accès invalide");

        view.setModel(nullptr);
        host.hide();
        {
            QSettings iso;
            iso.clear();
            iso.sync();
        }
        QProcess::execute("reg", { "delete", QStringLiteral("HKCU\\Software\\%1").arg(isoOrg), "/f" });
        auto& style = AnnotationStyle::instance();
        style.setMode(savedStyle.mode);
        style.setBasePixels(savedStyle.basePixels);
        style.setModelHeight(savedStyle.modelHeight);
        QCoreApplication::setOrganizationName(savedOrg);
        QCoreApplication::setApplicationName(savedApp);
        std::cout << "[PASS] Test 388: Menus d'affichage partagés (échelles, profils, diagnostic)" << std::endl;
        ++passed;
    }

    clip.clear();
    return true;
}
