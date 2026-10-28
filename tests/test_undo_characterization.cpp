// =====================================================================================================================
// Caractérisation Annuler / Rétablir AVANT la migration des appels Model::pushUndoState vers EditTransaction (TSA Core,
// étape 2, non faite) — suite « undo », tests 513 à 519. Les attentes décrivent le comportement ACTUEL (lignes citées,
// 39155c4). Une attente marquée « DÉFAUT FIGÉ » décrit un comportement discutable conservé volontairement : la migration
// qui le corrige doit modifier ce test explicitement.
// Non atteints (couche application, non liée aux tests) : MainWindow_Mesh.cpp:326 et :359, MainWindow_Tools.cpp:464.
// =====================================================================================================================

#include "test_common.h"
#include "undo_characterization.h"

#include <algorithm>

#include "AI/Tools/AIToolRegistry.h"
#include "Analysis/ResultsModel.h"
#include "Analysis/ResultsValidityGuard.h"
#include "Annotation/DimensionService.h"
#include "Commands/ModifyCommands.h"
#include "Interaction/ElementCreation.h"
#include "Library/LibraryManager.h"
#include "Model/CreationPresets.h"
#include "UndoRedo/CommandManager.h"
#include "Model/Load/MemberLoadCommands.h"
#include "UI/Dialogs/GridDialog.h"
#include "UI/Dialogs/GridSettingsDialog.h"
#include "UI/Dialogs/LoadCaseDialog.h"
#include "UI/Dialogs/NewNodeDialog.h"
#include "UI/Dialogs/NodalLoadDialog.h"
#include "UI/Properties/BeamPropertiesView.h"
#include "UI/Properties/CablePropertiesView.h"
#include "UI/Properties/ColumnPropertiesView.h"
#include "UI/Properties/FoundationPropertiesView.h"
#include "UI/Properties/LoadPropertiesView.h"
#include "UI/Properties/MemberEndNodesWidget.h"
#include "UI/Properties/NodePropertiesView.h"
#include "UI/Properties/SlabPropertiesView.h"
#include "UI/Properties/TrussMemberPropertiesView.h"
#include "UI/Properties/WallPropertiesView.h"
#include "Viewer/OccView.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>

#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <thread>

using UndoChar::CoalesceClock;
using UndoChar::UndoProbe;
using UndoChar::fingerprint;
using UndoChar::firstDifference;

namespace
{

/// Répond aux boîtes modales ouvertes par le code testé (convention des tests 498 et 508 : QTimer 20 ms).
struct ModalAutoAnswer
{
    QMessageBox::StandardButton answer = QMessageBox::Yes;
    bool acceptInputs = true;
    int messageBoxes = 0, inputDialogs = 0;
    QTimer timer;
    ModalAutoAnswer()
    {
        QObject::connect(&timer, &QTimer::timeout, [this] {
            for (auto* w : QApplication::topLevelWidgets())
            {
                if (!w->isVisible()) continue;
                if (auto* box = qobject_cast<QMessageBox*>(w))
                {
                    ++messageBoxes;
                    if (auto* b = box->button(answer)) b->click();
                    else if (auto* b2 = box->button(QMessageBox::Ok)) b2->click();
                    else box->reject();
                }
                else if (auto* input = qobject_cast<QInputDialog*>(w))
                {
                    ++inputDialogs;
                    acceptInputs ? input->accept() : input->reject();
                }
            }
        });
        timer.start(20);
    }
    ~ModalAutoAnswer() { timer.stop(); }
};

/// Champ numérique unique dont la valeur affichée vaut v (identification sans accès aux membres privés).
QDoubleSpinBox* spinWithValue(QWidget& w, double v)
{
    QDoubleSpinBox* found = nullptr;
    for (auto* s : w.findChildren<QDoubleSpinBox*>())
        if (!s->isHidden() && std::abs(s->value() - v) < 1e-9)
        {
            if (found) return nullptr; // ambigu : le test doit choisir des valeurs distinctes
            found = s;
        }
    return found;
}
QDoubleSpinBox* spinWithRange(QWidget& w, double lo, double hi)
{
    for (auto* s : w.findChildren<QDoubleSpinBox*>())
        if (s->minimum() == lo && s->maximum() == hi && !s->isHidden()) return s;
    return nullptr;
}
QLineEdit* editWithText(QWidget& w, const std::string& text)
{
    for (auto* e : w.findChildren<QLineEdit*>())
        if (e->text() == QString::fromStdString(text)) return e;
    return nullptr;
}
bool selectRowWithText(QWidget& w, const QString& text)
{
    for (auto* t : w.findChildren<QTableWidget*>())
        for (int r = 0; r < t->rowCount(); ++r)
            for (int c = 0; c < t->columnCount(); ++c)
                if (t->item(r, c) && t->item(r, c)->text() == text)
                {
                    t->setCurrentCell(r, 0);
                    return true;
                }
    return false;
}

std::shared_ptr<TSA::Analysis::ResultsModel> validResults()
{
    auto r = std::make_shared<TSA::Analysis::ResultsModel>();
    r->setValid(true);
    return r;
}

/// Contrôle commun « Annuler rend l'état d'avant, Rétablir l'état d'après » (empreintes intra-exécution).
bool undoRedoRoundTrip(Model& m, const TSA::Grid::GridManager* gm, const std::string& before, const std::string& after,
                       const char* label)
{
    TEST_CHECK(m.undo(), label << " : Annuler possible");
    const std::string u = fingerprint(m, gm);
    TEST_CHECK(u == before, label << " : Annuler rend l'état initial (" << firstDifference(before, u) << ")");
    TEST_CHECK(m.redo(), label << " : Rétablir possible");
    const std::string r = fingerprint(m, gm);
    TEST_CHECK(r == after, label << " : Rétablir rend l'état modifié (" << firstDifference(after, r) << ")");
    return true;
}

std::string joinTexts(const std::vector<std::string>& v)
{
    std::string s;
    for (const auto& x : v) s += (s.empty() ? "" : " ; ") + x;
    return s.empty() ? std::string("<aucun>") : s;
}

/// Contrat d'une transition (undo_characterization.h §5) :
///  - `entries` entrées Annuler ; révision +`revOp` (-1 : relevé seulement), chaque incrément annoncé par ModelEdited ;
///  - `recordCount` EditRecord, dont le premier contient `recordText` (si non vide) ;
///  - si une entrée existe : Annuler rend l'état initial, Rétablir l'état obtenu, +1 révision chacun (un seul
///    ModelEdited), EditRecord identiques après Rétablir.
bool checkTransition(const UndoChar::Transition& t, long long entries, long long revOp, size_t recordCount,
                     const std::string& recordText, const std::string& label)
{
    const long long rev = static_cast<long long>(t.rOp - t.r0);
    if (revOp < 0)
        std::cout << "  [RELEVÉ] " << label << " : révision +" << rev << ", entrées " << t.entries << ", EditRecord "
                  << joinTexts(t.recordsOp) << ", notifications « " << t.specificOp << " »" << std::endl;
    TEST_CHECK(t.entries == entries, label << " : entrées Annuler " << t.entries << " (attendu " << entries << ")");
    TEST_CHECK(revOp < 0 || rev == revOp, label << " : révision +" << rev << " (attendu +" << revOp << ")");
    TEST_CHECK(t.editedOp == rev, label << " : chaque incrément de révision annoncé par ModelEdited (" << t.editedOp << " pour +" << rev << ")");
    TEST_CHECK(t.recordsOp.size() == recordCount && (recordText.empty() || (!t.recordsOp.empty() && t.recordsOp[0].find(recordText) != std::string::npos)),
               label << " : EditRecord " << joinTexts(t.recordsOp));
    if (entries <= 0) return true;
    TEST_CHECK(t.undone && t.fUndo == t.f0, label << " : Annuler rend l'état initial (" << firstDifference(t.f0, t.fUndo) << ")");
    TEST_CHECK(t.rUndo == t.rOp + 1 && t.editedUndo == 1, label << " : Annuler = révision +" << (t.rUndo - t.rOp) << ", " << t.editedUndo << " ModelEdited");
    TEST_CHECK(t.redone && t.fRedo == t.fOp, label << " : Rétablir rend l'état obtenu (" << firstDifference(t.fOp, t.fRedo) << ")");
    TEST_CHECK(t.rRedo == t.rUndo + 1 && t.editedRedo == 1, label << " : Rétablir = révision +" << (t.rRedo - t.rUndo) << ", " << t.editedRedo << " ModelEdited");
    TEST_CHECK(t.recordsRedo == t.recordsOp, label << " : EditRecord conservés par Annuler / Rétablir (" << joinTexts(t.recordsRedo) << ")");
    return true;
}

/// Empreinte sans les lignes qui commencent par `prefix`.
std::string withoutLines(const std::string& fp, const std::string& prefix)
{
    std::istringstream in(fp);
    std::string line, out;
    while (std::getline(in, line))
        if (line.rfind(prefix, 0) != 0) out += line + "\n";
    return out;
}

/// Toutes les lignes de deux empreintes sont égales, sauf celles qui commencent par `prefix`.
bool sameExceptPrefix(const std::string& a, const std::string& b, const std::string& prefix)
{
    return withoutLines(a, prefix) == withoutLines(b, prefix);
}

/// Première différence en ignorant les lignes qui commencent par `prefix` (diagnostic de sameExceptPrefix).
std::string firstDifferenceExcept(const std::string& a, const std::string& b, const std::string& prefix)
{
    return firstDifference(withoutLines(a, prefix), withoutLines(b, prefix));
}

/// Ligne d'empreinte commençant par `prefix` (vide si absente).
std::string lineWithPrefix(const std::string& fp, const std::string& prefix)
{
    std::istringstream in(fp);
    std::string line;
    while (std::getline(in, line))
        if (line.rfind(prefix, 0) == 0) return line;
    return {};
}

} // namespace

bool runSuite_UndoCharacterization(int& passed)
{
    // -----------------------------------------------------------------------------------------------------------------
    // TEST 513 : infrastructure — pushUndoState seul, ordre des notifications, coalescence (cas déterministes),
    // transactions (commit, rollback, destructeur, exception, discardUnchanged, imbrication), empreinte.
    // -----------------------------------------------------------------------------------------------------------------
    {
        Model m;
        UndoChar::prepareModel(m);
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(6, 0, 0);
        const int b = m.addBar(n1, n2, Section::ipe(300), Material::steelS355(), BarRole::Beam);
        m.clearUndoRedo();

        // a) pushUndoState seul : 1 entrée, révision +1, un seul ModelEdited, aucune notification spécifique
        //    (Model_Snapshots.cpp:10-21 → UndoManager.cpp:91-96 → pushEntry:65 → Model.h:414-418 → Model.cpp:35-42).
        {
            UndoProbe p(m);
            m.pushUndoState("Action A");
            TEST_CHECK(p.undoDelta() == 1 && p.revDelta() == 1 && p.obs.fullSequence() == "ModelEdited" && m.isModified(),
                       "Test 513: pushUndoState seul (" << p.obs.fullSequence() << ")");
        }
        // b) Ordre actuel : bumpRevision (ModelEdited à tous) PUIS la notification spécifique (Model.cpp, notify*Modified) ;
        //    charges : paire spécifique + générique par observateur (Model.cpp:1211-1219).
        {
            UndoProbe p(m);
            m.notifyBeamModified(b);
            const int lid = m.loadManager().addNodalLoad(NodalLoad(0, n2, 1, 0, 0, -10));
            m.notifyNodalLoadAdded(lid);
            TEST_CHECK(p.obs.fullSequence() == "ModelEdited BeamModified(" + std::to_string(b) + ") ModelEdited NodalLoadAdded(" +
                                                   std::to_string(lid) + ") LoadAdded(" + std::to_string(lid) + ")",
                       "Test 513: ordre ModelEdited → spécifique (" << p.obs.fullSequence() << ")");
            TEST_CHECK(p.obs.events[1].revision == p.rev0 + 1, "Test 513: révision déjà incrémentée lors de la notification spécifique");
            m.loadManager().removeNodalLoad(lid);
        }
        // c) Coalescence (UndoManager.cpp:78-89) — cas déterministes, et « même clé » sous garde temporelle.
        {
            m.clearUndoRedo();
            UndoProbe p(m);
            CoalesceClock clock;
            m.pushUndoState("Modification Barre 1", "Modification Barre 1");
            m.pushUndoState("Modification Barre 1", "Modification Barre 1");
            if (clock.safelyInside())
                TEST_CHECK(p.undoDelta() == 1 && p.revDelta() == 2, "Test 513: même clé, immédiat → 1 entrée, révision +2");
            else
                std::cout << "[SKIP] Test 513: coalescence « même clé » non vérifiable (intervalle ≥ 1,5 s)" << std::endl;
            m.pushUndoState("Autre", "Autre clé");
            m.pushUndoState("Sans clé");
            m.pushUndoState("Sans clé");
            if (clock.safelyInside())
                TEST_CHECK(p.undoDelta() == 4, "Test 513: clé différente ou vide → nouvelle entrée");
            else
                std::cout << "[SKIP] Test 513: clé différente ou vide non vérifiable (intervalle ≥ 1,5 s)" << std::endl;
        }
        {
            // Pile Rétablir non vide → jamais de coalescence (condition m_redoStack.empty(), UndoManager.cpp:79).
            m.clearUndoRedo();
            m.pushUndoState("K", "K");
            m.getBeam(b)->setRotation(5.0);
            m.pushUndoState("K2", "K2");
            TEST_CHECK(m.undo(), "Test 513: undo préalable");
            UndoProbe p(m);
            m.pushUndoState("K", "K");
            TEST_CHECK(p.undoDelta() == 1 && p.redoDelta() == -1, "Test 513: après Annuler, même clé → nouvelle entrée, Rétablir vidé");
        }
        {
            // Entrée recréée par Rétablir : clé perdue (UndoManager.cpp:136-143, undoEntry sans coalesceKey).
            m.clearUndoRedo();
            m.pushUndoState("R", "R");
            TEST_CHECK(m.undo() && m.redo(), "Test 513: undo/redo préalables");
            UndoProbe p(m);
            m.pushUndoState("R", "R");
            TEST_CHECK(p.undoDelta() == 1, "Test 513: après Rétablir, même clé → nouvelle entrée (clé non conservée)");
        }
        {
            // Entrée de transaction : sans clé (beginTransaction ne reçoit qu'un nom, UndoManager.cpp:166-174 ;
            // EditTransaction.h:30-38) → un push à clé ne s'y fond jamais.
            m.clearUndoRedo();
            {
                TSA::UndoRedo::EditTransaction tx(m, "Tx");
                m.getBeam(b)->setRotation(7.0);
                tx.commit();
            }
            UndoProbe p(m);
            m.pushUndoState("Tx", "Tx");
            TEST_CHECK(p.undoDelta() == 1, "Test 513: entrée de transaction sans clé de coalescence");
        }
        {
            // Fenêtre expirée (≥ 2 s) → nouvelle entrée. Coût : 2,1 s ; vérification déterministe (attente mesurée).
            m.clearUndoRedo();
            m.pushUndoState("W", "W");
            const auto t0 = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(2100));
            TEST_CHECK(std::chrono::steady_clock::now() - t0 >= TSA::UndoRedo::UndoManager::COALESCE_WINDOW, "Test 513: attente mesurée ≥ 2 s");
            UndoProbe p(m);
            m.pushUndoState("W", "W");
            TEST_CHECK(p.undoDelta() == 1, "Test 513: fenêtre expirée → nouvelle entrée");
        }

        // d) Transactions (C5). Résultats suivis par ResultsValidityGuard (Analysis/ResultsValidityGuard.cpp:58-71).
        auto guardCase = [&](const char* label, const std::function<void()>& op, long long undo, bool modifiedAfter, bool stale,
                             bool sameState, const std::string& diffPayload) -> bool {
            m.clearUndoRedo();
            m.setModified(false);
            TSA::Analysis::ResultsValidityGuard guard(&m);
            int staleCalls = 0;
            guard.setStaleCallback([&] { ++staleCalls; });
            guard.trackResults(validResults());
            const std::string before = fingerprint(m);
            UndoProbe p(m);
            op();
            TEST_CHECK(p.undoDelta() == undo, label << " : entrées Annuler " << p.undoDelta());
            TEST_CHECK(m.isModified() == modifiedAfter, label << " : isModified " << m.isModified());
            TEST_CHECK((staleCalls == 1) == stale && guard.resultsUpToDate() == !stale, label << " : résultats périmés " << staleCalls);
            TEST_CHECK((fingerprint(m) == before) == sameState, label << " : état");
            if (!diffPayload.empty())
            {
                // Le différentiel n'est pas forcément le dernier événement (un commit ultérieur émet ModelEdited).
                const auto diffEvent = std::find_if(p.obs.events.begin(), p.obs.events.end(),
                                                    [](const UndoChar::Event& e) { return e.kind == "ModelDiffApplied"; });
                TEST_CHECK(p.obs.count("ModelDiffApplied") == 1 && diffEvent != p.obs.events.end() &&
                               diffEvent->payload == diffPayload,
                           label << " : différentiel publié " << p.obs.specificSequence());
            }
            else
                TEST_CHECK(p.obs.count("ModelDiffApplied") == 0, label << " : aucun différentiel");
            return true;
        };
        using TSA::UndoRedo::EditTransaction;
        const std::string bs = std::to_string(b);
        if (!guardCase("Test 513: commit avec modification", [&] {
                EditTransaction tx(m, "rot");
                m.getBeam(b)->setRotation(11.0);
                tx.commit();
            }, 1, true, true, false, ""))
            return false;
        if (!guardCase("Test 513: commit sans modification (BUG-127 : ni entrée, ni révision, ni invalidation)", [&] {
                EditTransaction tx(m, "rien");
                tx.commit();
            }, 0, false, false, true, ""))
            return false;
        if (!guardCase("Test 513: rollback explicite après modification", [&] {
                EditTransaction tx(m, "annulée");
                m.getBeam(b)->setRotation(12.0);
                tx.rollback();
            }, 0, false, true, true, "B{c:;m:" + bs + ";d:}"))
            return false;
        if (!guardCase("Test 513: rollback sans modification (BUG-127 : ni différentiel, ni révision, isModified inchangé)", [&] {
                EditTransaction tx(m, "vide");
                tx.rollback();
            }, 0, false, false, true, ""))
            return false;
        if (!guardCase("Test 513: destructeur sans commit", [&] {
                EditTransaction tx(m, "abandon");
                m.getBeam(b)->setRotation(13.0);
            }, 0, false, true, true, "B{c:;m:" + bs + ";d:}"))
            return false;
        if (!guardCase("Test 513: exception dans la transaction", [&] {
                try
                {
                    EditTransaction tx(m, "exception");
                    m.getBeam(b)->setRotation(14.0);
                    throw std::runtime_error("échec");
                }
                catch (const std::runtime_error&) {}
            }, 0, false, true, true, "B{c:;m:" + bs + ";d:}"))
            return false;
        if (!guardCase("Test 513: discardUnchanged", [&] {
                EditTransaction tx(m, "inchangé");
                tx.discardUnchanged();
            }, 0, false, false, true, ""))
            return false;
        if (!guardCase("Test 513: modification sous les tolérances de ModelDiff (1e-9°) : entrée créée (comparaison exacte)", [&] {
                EditTransaction tx(m, "infime");
                m.getBeam(b)->setRotation(m.getBeam(b)->rotation() + 1e-9);
                tx.commit();
            }, 1, true, true, false, ""))
            return false;
        if (!guardCase("Test 513: modifier puis restaurer, commit (BUG-127 : aucune entrée)", [&] {
                EditTransaction tx(m, "aller-retour");
                const double r0 = m.getBeam(b)->rotation();
                m.getBeam(b)->setRotation(r0 + 1.0);
                m.getBeam(b)->setRotation(r0);
                tx.commit();
            }, 0, false, false, true, ""))
            return false;
        if (!guardCase("Test 513: imbrication, rollback interne puis commit externe", [&] {
                EditTransaction outer(m, "externe");
                m.getBeam(b)->setRotation(15.0);
                {
                    EditTransaction inner(m, "interne");
                    inner.rollback(); // état de début de la transaction englobante rétabli ; elle reste ouverte
                }
                outer.commit();   // rien ne diffère du début : aucune entrée
            }, 0, false, true, true, "B{c:;m:" + bs + ";d:}"))
            return false;
        if (!guardCase("Test 513: imbrication, rollback interne, nouvelle modification, commit externe (BUG-127 (3))", [&] {
                EditTransaction outer(m, "externe");
                m.getBeam(b)->setRotation(16.0);
                {
                    EditTransaction inner(m, "interne");
                    inner.rollback();
                }
                m.getBeam(b)->setRotation(17.0);   // autrefois hors historique : profondeur remise à 0 par le rollback
                outer.commit();
            }, 1, true, true, false, "B{c:;m:" + bs + ";d:}"))
            return false;
        {
            // pushUndoState absorbé dans une transaction : révision +1 par appel (UndoManager.cpp:70-76).
            m.clearUndoRedo();
            UndoProbe p(m);
            {
                EditTransaction tx(m, "absorbe");
                m.pushUndoState("x", "x");
                m.pushUndoState("y");
                m.getBeam(b)->setRotation(21.0);
                TEST_CHECK(!m.canUndo(), "Test 513: Annuler indisponible pendant une transaction (UndoManager.h:41)");
                tx.commit();
            }
            TEST_CHECK(p.undoDelta() == 1 && p.revDelta() == 3 && p.topName() == "absorbe",
                       "Test 513: pushUndoState absorbés (révision " << p.revDelta() << ")");
        }

        // e) Empreinte : déterminisme entre deux modèles construits à l'identique, sensibilité, restauration par Annuler.
        {
            auto build = [](Model& x) {
                const int a = x.addNode(0, 0, 0, "", "A"), c = x.addNode(5, 0, 0, "", "B");
                x.addBar(a, c, Section::ipe(240), Material::steelS235(), BarRole::Beam, 0.1);
                x.loadManager().addNodalLoad(NodalLoad(0, c, 1, 0, 0, -12.5));
            };
            Model m1, m2;
            build(m1);
            build(m2);
            UndoChar::FingerprintOptions portable;
            portable.includeVolatileIds = false;
            const std::string f1 = fingerprint(m1, nullptr, portable);
            TEST_CHECK(f1 == fingerprint(m2, nullptr, portable), "Test 513: empreinte identique pour deux constructions identiques (" <<
                                                                    firstDifference(f1, fingerprint(m2, nullptr, portable)) << ")");
            TEST_CHECK(fingerprint(m1) == fingerprint(m1), "Test 513: empreinte stable (aucun effet de la prise d'empreinte)");
            m1.getNode(m1.nodes().begin()->first)->setCoordinates(0, 0, 1e-12);
            TEST_CHECK(fingerprint(m1, nullptr, portable) != f1, "Test 513: une variation de 1e-12 est visible (%a, sans tolérance)");
        }
        std::cout << "[PASS] Test 513: Caractérisation Annuler : historique, coalescence, transactions, empreinte" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 514 : F1 — panneaux de propriétés (10 appels : 8 vues d'éléments + LoadPropertiesView ×2).
    // -----------------------------------------------------------------------------------------------------------------
    {
        ModalAutoAnswer modal;
        Model m;
        UndoChar::prepareModel(m);
        const int n1 = m.addNode(0, 0, 0, "", "N_a"), n2 = m.addNode(6, 0, 0, "", "N_b"), n3 = m.addNode(6, 4, 0), n4 = m.addNode(0, 4, 0);
        const int n5 = m.addNode(0, 0, 3), n6 = m.addNode(6, 0, 3);
        const int beam = m.addBar(n1, n2, Section::rectangular(0.25, 0.55), Material::steelS355(), BarRole::Beam, 12.5, "B_test");
        const int col = m.addColumn(n1, n5, Section::rectangular(0.35, 0.45), Material::concreteC25_30(), 0.0, "C_test");
        const int slab = m.addSlab({ n1, n2, n3, n4 }, 0.175, "S_test");
        const int wall = m.addWall(n5, n6, 3.25, 0.22, "W_test"); // valeurs exactes à 2 décimales (le champ arrondit à l'affichage)
        const int found = m.addFoundation(n3, 1.55, 1.75, 0.65, "F_test");
        const int truss = m.addTrussMember(n2, n5, 0.11, "T_test");
        const int cable = m.addCable(n4, n6, 0.021, "K_test");
        m.clearUndoRedo();

        // Matrice commune aux 8 vues d'élément (clé de coalescence = libellé « Modification <Type> <id> »).
        auto panel = [&](auto& view, int id, const std::string& modifiedKind, const std::string& name, double fieldValue,
                         double newValue, const char* label, bool noopKeepsState = true) -> bool {
            view.setElementId(id);
            const std::string fp0 = fingerprint(m);
            m.clearUndoRedo();
            // 1. Valider sans rien changer : 1 entrée (DÉFAUT FIGÉ), une notification spécifique, état inchangé.
            {
                UndoProbe p(m);
                CoalesceClock clock;
                view.applyChanges();
                TEST_CHECK(p.undoDelta() == 1 && p.obs.count(modifiedKind, id) == 1, label << " : valider sans changement → 1 entrée, 1 " << modifiedKind
                                                                                              << " (" << p.obs.specificSequence() << ")");
                TEST_CHECK((fingerprint(m) == fp0) == noopKeepsState,
                           label << " : valider sans changement " << (noopKeepsState ? "ne réécrit rien" : "RÉÉCRIT l'état (DÉFAUT FIGÉ)") << " ("
                                 << firstDifference(fp0, fingerprint(m)) << ")");
                // 2. Cran immédiat sur un champ : fondu dans la même entrée (garde temporelle).
                if (fieldValue == fieldValue) // NaN = pas de champ numérique testé pour cette vue
                {
                    QDoubleSpinBox* spin = spinWithValue(view, fieldValue);
                    TEST_CHECK(spin != nullptr, label << " : champ de valeur " << fieldValue << " trouvé (unique)");
                    spin->setValue(newValue); // valueChanged → onWidgetChanged → applyChanges
                    if (clock.safelyInside())
                        TEST_CHECK(p.undoDelta() == 1, label << " : cran fondu dans la même entrée (coalescence)");
                    else
                        std::cout << "[SKIP] " << label << " : coalescence non vérifiable (lenteur)" << std::endl;
                    TEST_CHECK(fingerprint(m) != fp0, label << " : champ appliqué");
                }
            }
            const std::string fp1 = fingerprint(m);
            if (fieldValue == fieldValue && !undoRedoRoundTrip(m, nullptr, fp0, fp1, label)) return false;
            // 3. Élément supprimé entre-temps : sortie anticipée AVANT le push (aucune entrée, aucune notification).
            {
                (void)name;
                UndoProbe p(m);
                view.setElementId(999999);
                view.applyChanges();
                TEST_CHECK(p.undoDelta() == 0 && p.obs.events.empty(), label << " : élément introuvable → aucun effet");
            }
            return true;
        };
        const double noField = std::numeric_limits<double>::quiet_NaN();
        {
            TSA::UI::BeamPropertiesView v(&m);
            if (!panel(v, beam, "BeamModified", "B_test", 12.5, 20.0, "Test 514: poutre (BeamPropertiesView.cpp:424)")) return false;
        }
        {
            TSA::UI::ColumnPropertiesView v(&m);
            if (!panel(v, col, "ColumnModified", "C_test", 0.35, 0.40, "Test 514: poteau (ColumnPropertiesView.cpp:345)")) return false;
        }
        {
            TSA::UI::SlabPropertiesView v(&m);
            if (!panel(v, slab, "SlabModified", "S_test", 0.175, 0.25, "Test 514: dalle (SlabPropertiesView.cpp:190)")) return false;
        }
        {
            TSA::UI::WallPropertiesView v(&m);
            if (!panel(v, wall, "WallModified", "W_test", 3.25, 3.5, "Test 514: voile (WallPropertiesView.cpp:184)")) return false;
        }
        {
            TSA::UI::FoundationPropertiesView v(&m);
            if (!panel(v, found, "FoundationModified", "F_test", 1.75, 2.0, "Test 514: fondation (FoundationPropertiesView.cpp:163)")) return false;
        }
        {
            TSA::UI::TrussMemberPropertiesView v(&m);
            if (!panel(v, truss, "TrussMemberModified", "T_test", noField, 0, "Test 514: treillis (TrussMemberPropertiesView.cpp:174)")) return false;
        }
        {
            TSA::UI::CablePropertiesView v(&m);
            // Câble : applyChanges réécrit tous les champs depuis l'affichage arrondi (CablePropertiesView.cpp:275-280). CONSTATÉ
            // (essai local 2026-10-26) : valider sans rien changer modifie l'aire de la définition (dArea 0x1.6b2f7682c2457p-12 →
            // 0x1.6b3a0abdc923cp-12 pour Ø 21 mm) — DÉFAUT FIGÉ.
            if (!panel(v, cable, "CableModified", "K_test", noField, 0, "Test 514: câble (CablePropertiesView.cpp:269)", false)) return false;
        }
        {
            TSA::UI::NodePropertiesView v(&m);
            // CONSTATÉ (essai local) : valider sans changement impose la couleur par défaut « #2563EB » à un nœud sans couleur
            // (NodePropertiesView.cpp:269 et 526 ; même défaut que celui corrigé pour les poteaux au test 221) — DÉFAUT FIGÉ.
            if (!panel(v, n4, "NodeModified", "", 4.0, 4.5, "Test 514: nœud (NodePropertiesView.cpp:522)", false)) return false;
            // Donnée invalide : n1 sur n2 (poutre de longueur nulle) → refus AVANT le push (NodePropertiesView.cpp:510-517).
            v.setElementId(n1);
            const std::string fp = fingerprint(m);
            UndoProbe p(m);
            const int boxes = modal.messageBoxes;
            QDoubleSpinBox* xSpin = spinWithRange(v, -100000.0, 100000.0); // premier champ de cette plage = X (NodePropertiesView.cpp:64-65)
            TEST_CHECK(xSpin != nullptr, "Test 514: nœud — champ X trouvé");
            xSpin->setValue(6.0);
            TEST_CHECK(p.undoDelta() == 0 && fingerprint(m) == fp && modal.messageBoxes > boxes,
                       "Test 514: nœud — coordonnées refusées : aucune entrée, état inchangé, avertissement affiché");
        }
        {
            // Exception pendant la notification (observateur) : comportement actuel = entrée créée, mutation conservée.
            TSA::UI::BeamPropertiesView v(&m);
            v.setElementId(beam);
            m.clearUndoRedo();
            UndoProbe p(m);
            p.obs.throwOnKind = "BeamModified";
            QLineEdit* nameEdit = editWithText(v, "B_test");
            TEST_CHECK(nameEdit != nullptr, "Test 514: champ du nom de la poutre trouvé");
            nameEdit->setText("B_exception");
            bool thrown = false;
            try { v.applyChanges(); } catch (const std::runtime_error&) { thrown = true; }
            TEST_CHECK(thrown && p.undoDelta() == 1 && m.getBeam(beam)->name() == "B_exception",
                       "Test 514: exception après mutation — entrée présente, mutation conservée (DÉFAUT FIGÉ : pas de rollback)");
            TEST_CHECK(m.undo() && m.getBeam(beam)->name() == "B_test", "Test 514: Annuler après exception rend l'état initial");
        }
        {
            // ResultsValidityGuard : le push seul suffit à périmer les résultats (Model.cpp:35-42 via setModified).
            TSA::Analysis::ResultsValidityGuard guard(&m);
            int stale = 0;
            guard.setStaleCallback([&] { ++stale; });
            guard.trackResults(validResults());
            TSA::UI::WallPropertiesView v(&m);
            v.setElementId(wall);
            v.applyChanges();
            TEST_CHECK(stale == 1 && !guard.resultsUpToDate(), "Test 514: résultats périmés une fois");
        }

        // LoadPropertiesView : charge nodale (339, sans clé, sans contrôle) et sur barre (380, validée, sans entrée si inchangée).
        const int nl = m.loadManager().addNodalLoad(NodalLoad(0, n2, 1, 0, 0, -10, 0, 0, 0, LoadCoordSystem::Global, "NL_test"));
        const int ml = m.loadManager().addMemberLoad(MemberLoad::uniform(0, beam, 1, -5.0, LoadDirection::Gravity));
        m.clearUndoRedo();
        {
            TSA::UI::LoadPropertiesView v(&m);
            v.setNodalLoadId(nl);
            UndoProbe p(m);
            v.applyChanges();
            v.applyChanges();
            TEST_CHECK(p.undoDelta() == 2 && p.obs.count("NodalLoadModified", nl) == 2 && p.obs.count("LoadModified", nl) == 2,
                       "Test 514: charge nodale — chaque validation crée une entrée (aucune clé, aucun contrôle : DÉFAUT FIGÉ)");
        }
        {
            TSA::UI::LoadPropertiesView v(&m);
            v.setMemberLoadId(ml);
            const std::string fp0 = fingerprint(m);
            UndoProbe p(m);
            v.applyChanges();
            TEST_CHECK(p.undoDelta() == 0 && p.obs.events.empty(), "Test 514: charge sur barre inchangée → aucune entrée (LoadPropertiesView.cpp:375-378)");
            const int boxes = modal.messageBoxes;
            QDoubleSpinBox* q1Spin = spinWithValue(v, -5.0);
            TEST_CHECK(q1Spin != nullptr, "Test 514: champ q1 de la charge sur barre trouvé");
            q1Spin->setValue(-8.0); // q1 ; editingFinished non émis : validation explicite
            v.applyChanges();
            TEST_CHECK(p.undoDelta() == 1 && p.obs.count("MemberLoadModified", ml) == 1 && p.obs.count("LoadModified", ml) == 1,
                       "Test 514: charge sur barre modifiée → 1 entrée, notifications");
            const std::string fp1 = fingerprint(m);
            if (!undoRedoRoundTrip(m, nullptr, fp0, fp1, "Test 514: charge sur barre")) return false;
            TEST_CHECK(modal.messageBoxes == boxes, "Test 514: aucune boîte pour une valeur valide");
        }
        std::cout << "[PASS] Test 514: F1 panneaux de propriétés (entrées, coalescence, notifications, refus, exception)" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 515 : F2 — chargements (LoadCaseDialog ×5, NodalLoadDialog, MemberLoadCommands ×2 ; MainWindow_Mesh:326 exclu).
    // -----------------------------------------------------------------------------------------------------------------
    {
        ModalAutoAnswer modal;
        Model m;
        UndoChar::prepareModel(m);
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(5, 0, 0);
        const int b = m.addBar(n1, n2, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        m.clearUndoRedo();
        TSA::UI::LoadCaseDialog dlg(&m);

        // Ajout de cas (214) : saisies acceptées → 1 entrée, LoadCaseChanged(actif) une fois.
        {
            UndoProbe p(m);
            const std::string fp0 = fingerprint(m);
            QMetaObject::invokeMethod(&dlg, "onAddLoadCase", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 1 && p.obs.specificSequence() == "LoadCaseChanged(" + std::to_string(m.loadManager().activeLoadCaseId()) + ")",
                       "Test 515: cas ajouté (" << p.obs.specificSequence() << ")");
            const std::string fp1 = fingerprint(m);
            // Annuler : le différentiel ne couvre pas les charges (ModelDiff.h:38-90) → publié VIDE.
            p.obs.clear();
            TEST_CHECK(m.undo() && fingerprint(m) == fp0 && p.obs.count("ModelDiffApplied") == 1 && p.obs.events.back().payload == "<vide>",
                       "Test 515: Annuler l'ajout — état rendu, différentiel vide");
            TEST_CHECK(m.redo() && fingerprint(m) == fp1, "Test 515: Rétablir l'ajout");
        }
        // Saisie annulée : sortie AVANT le push (202).
        {
            modal.acceptInputs = false;
            UndoProbe p(m);
            QMetaObject::invokeMethod(&dlg, "onAddLoadCase", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 0 && p.obs.events.empty(), "Test 515: saisie annulée → aucun effet");
            modal.acceptInputs = true;
        }
        // Suppression d'un cas portant des charges (231) : charges supprimées SANS notification Removed (DÉFAUT FIGÉ, C6).
        {
            const int q = 2; // cas « Q » des valeurs par défaut (LoadManager.cpp:37)
            const int nl = m.loadManager().addNodalLoad(NodalLoad(0, n2, q, 0, 0, -3));
            const int ml = m.loadManager().addMemberLoad(MemberLoad::uniform(0, b, q, -2.0, LoadDirection::Gravity));
            m.clearUndoRedo();
            const std::string fp0 = fingerprint(m);
            TSA::UI::LoadCaseDialog fresh(&m); // tableau relu à la construction
            TEST_CHECK(selectRowWithText(fresh, QStringLiteral("Q")), "Test 515: ligne du cas Q sélectionnée");
            UndoProbe p(m);
            modal.answer = QMessageBox::Yes;
            QMetaObject::invokeMethod(&fresh, "onRemoveLoadCase", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 1 && !m.loadManager().getNodalLoad(nl) && !m.loadManager().getMemberLoad(ml), "Test 515: cas et charges supprimés");
            TEST_CHECK(p.obs.count("NodalLoadRemoved") == 0 && p.obs.count("MemberLoadRemoved") == 0 && p.obs.count("LoadCaseChanged") == 1,
                       "Test 515: seule LoadCaseChanged est émise (DÉFAUT FIGÉ : " << p.obs.specificSequence() << ")");
            TEST_CHECK(m.undo() && fingerprint(m) == fp0, "Test 515: Annuler rend le cas et ses charges");
        }
        // Suppression refusée (question → Non) : aucune entrée.
        {
            TSA::UI::LoadCaseDialog fresh(&m);
            TEST_CHECK(selectRowWithText(fresh, QStringLiteral("W")), "Test 515: ligne du cas W sélectionnée");
            modal.answer = QMessageBox::No;
            UndoProbe p(m);
            const int boxesBefore = modal.messageBoxes;
            QMetaObject::invokeMethod(&fresh, "onRemoveLoadCase", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 0 && p.obs.events.empty() && modal.messageBoxes > boxesBefore,
                       "Test 515: suppression refusée (question posée, réponse Non) → aucun effet");
            modal.answer = QMessageBox::Yes;
            // Réinitialisation Eurocodes (247) : toutes les charges effacées, une seule LoadCaseChanged.
            m.loadManager().addNodalLoad(NodalLoad(0, n2, 1, 0, 0, -1));
            UndoProbe p2(m);
            QMetaObject::invokeMethod(&fresh, "onResetEurocodes", Qt::DirectConnection);
            TEST_CHECK(p2.undoDelta() == 1 && m.loadManager().nodalLoads().empty() && p2.obs.count("NodalLoadRemoved") == 0,
                       "Test 515: réinitialisation — charges effacées sans notification Removed (DÉFAUT FIGÉ)");
            // Combinaison : création (263) → 1 entrée, LoadCaseChanged seule, Annuler / Rétablir.
            const size_t combos0 = m.loadManager().combinations().size();
            const int active = m.loadManager().activeLoadCaseId();
            const std::string fpc0 = fingerprint(m);
            UndoProbe p4(m);
            QMetaObject::invokeMethod(&fresh, "onAddCombination", Qt::DirectConnection);
            TEST_CHECK(p4.undoDelta() == 1 && m.loadManager().combinations().size() == combos0 + 1 &&
                           p4.obs.specificSequence() == "LoadCaseChanged(" + std::to_string(active) + ")",
                       "Test 515: combinaison créée (" << p4.obs.specificSequence() << ")");
            const std::string fpc1 = fingerprint(m);
            if (!undoRedoRoundTrip(m, nullptr, fpc0, fpc1, "Test 515: création de combinaison")) return false;
            // Suppression d'une combinaison sélectionnée (280) → 1 entrée, Annuler / Rétablir.
            // Tableau et ligne identifiés par le contenu (id en colonne 0, nom en colonne 1, LoadCaseDialog.cpp:177-178) :
            // l'ordre de findChildren n'est pas garanti.
            const auto& lastCombo = *m.loadManager().combinations().rbegin();
            QTableWidget* comboTable = nullptr;
            int comboRow = -1;
            for (auto* t : fresh.findChildren<QTableWidget*>())
                for (int r = 0; r < t->rowCount(); ++r)
                    if (t->item(r, 0) && t->item(r, 1) && t->item(r, 0)->text() == QString::number(lastCombo.first) &&
                        t->item(r, 1)->text() == QString::fromStdString(lastCombo.second.name()))
                    {
                        comboTable = t;
                        comboRow = r;
                    }
            TEST_CHECK(comboTable != nullptr, "Test 515: ligne de la combinaison créée trouvée dans le tableau");
            comboTable->setCurrentCell(comboRow, 0);
            UndoProbe p5(m);
            QMetaObject::invokeMethod(&fresh, "onRemoveCombination", Qt::DirectConnection);
            TEST_CHECK(p5.undoDelta() == 1 && m.loadManager().combinations().size() == combos0, "Test 515: combinaison sélectionnée supprimée");
            const std::string fpc2 = fingerprint(m);
            if (!undoRedoRoundTrip(m, nullptr, fpc1, fpc2, "Test 515: suppression de combinaison")) return false;
            // Sans ligne sélectionnée → sortie avant le push (277).
            comboTable->setCurrentCell(-1, -1);
            UndoProbe p3(m);
            QMetaObject::invokeMethod(&fresh, "onRemoveCombination", Qt::DirectConnection);
            TEST_CHECK(p3.undoDelta() == 0, "Test 515: suppression de combinaison sans sélection → aucun effet");
        }
        // NodalLoadDialog (299) : composantes nulles refusées avant le push ; valide → 1 entrée + boîte « Succès » après mutation.
        {
            TSA::UI::NodalLoadDialog nd(&m);
            nd.setTargetNodeId(n2);
            UndoProbe p(m);
            QMetaObject::invokeMethod(&nd, "onApplyClicked", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 0 && p.obs.events.empty(), "Test 515: charge nodale nulle refusée");
            for (auto* s : nd.findChildren<QDoubleSpinBox*>())
                if (s->suffix() == QStringLiteral(" kN") && s->minimum() == -100000.0) { s->setValue(-7.0); break; }
            const std::string fp0 = fingerprint(m);
            QMetaObject::invokeMethod(&nd, "onApplyClicked", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 1 && p.obs.count("NodalLoadAdded") == 1 && p.obs.count("LoadAdded") == 1, "Test 515: charge nodale appliquée");
            const std::string fp1 = fingerprint(m);
            if (!undoRedoRoundTrip(m, nullptr, fp0, fp1, "Test 515: charge nodale")) return false;
        }
        // MemberLoadCommands (80, 107) : tout ou rien, une entrée, une notification par charge ; exception injectée.
        {
            using TSA::Model::applyMemberLoad;
            using TSA::Model::removeLoads;
            MemberLoad proto = MemberLoad::uniform(0, 0, 1, -4.0, LoadDirection::Gravity);
            std::string err;
            UndoProbe p(m);
            TEST_CHECK(applyMemberLoad(m, proto, { { b }, { 424242 } }, "x", &err).empty() && p.undoDelta() == 0 && p.obs.events.empty(),
                       "Test 515: cible invalide → aucun effet");
            p.obs.throwOnKind = "MemberLoadAdded";
            bool thrown = false;
            try { applyMemberLoad(m, proto, { { b }, { b } }, "deux", &err); } catch (const std::runtime_error&) { thrown = true; }
            TEST_CHECK(thrown && p.undoDelta() == 1 && m.loadManager().memberLoads().size() == 1,
                       "Test 515: exception après la 1re charge — entrée créée, application PARTIELLE (DÉFAUT FIGÉ)");
            TEST_CHECK(m.undo(), "Test 515: Annuler après exception");
            const int removed = removeLoads(m, { 99999 }, { 99999 }, "rien");
            TEST_CHECK(removed == 0 && p.undoDelta() == 0, "Test 515: removeLoads sans charge existante → aucune entrée (MemberLoadCommands.cpp:105)");
            // removeLoads avec charges existantes (107) : 1 entrée, une notification Removed par charge, Annuler / Rétablir.
            const std::vector<int> created = applyMemberLoad(m, proto, { { b }, { b } }, "deux", &err);
            TEST_CHECK(created.size() == 2, "Test 515: deux charges sur barre créées (" << err << ")");
            const std::string fpr0 = fingerprint(m);
            UndoProbe pr(m);
            const int removedNow = removeLoads(m, {}, { created[0], created[1] }, "retrait");
            TEST_CHECK(removedNow == 2 && pr.undoDelta() == 1 && pr.obs.count("MemberLoadRemoved") == 2 && m.loadManager().memberLoads().empty(),
                       "Test 515: removeLoads — 1 entrée, 2 MemberLoadRemoved (" << pr.obs.specificSequence() << ")");
            const std::string fpr1 = fingerprint(m);
            if (!undoRedoRoundTrip(m, nullptr, fpr0, fpr1, "Test 515: removeLoads")) return false;
        }
        std::cout << "[PASS] Test 515: F2 chargements (cas, combinaisons, charges nodales et sur barre)" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 516 : F3 — appuis (MemberEndNodesWidget.cpp:143 ; MainWindow_Tools.cpp:464 et MainWindow_Mesh.cpp:359 exclus).
    // -----------------------------------------------------------------------------------------------------------------
    {
        Model m;
        UndoChar::prepareModel(m);
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(0, 0, 3);
        m.addColumn(n1, n2);
        m.clearUndoRedo();
        TSA::UI::MemberEndNodesWidget w;
        w.setModel(&m);
        w.setNodes(n1, n2);
        QComboBox* start = w.findChildren<QComboBox*>().value(0);
        TEST_CHECK(start && start->isEnabled(), "Test 516: liste des appuis du nœud de début");
        auto choose = [&](TSA::Model::SupportType t) {
            const int i = start->findData(static_cast<int>(t));
            start->setCurrentIndex(i);
            emit start->activated(i); // signal utilisateur (MemberEndNodesWidget.cpp:71)
        };
        const std::string fp0 = fingerprint(m);
        UndoProbe p(m);
        choose(TSA::Model::SupportType::Fixed);
        TEST_CHECK(p.undoDelta() == 1 && p.obs.specificSequence() == "NodeModified(" + std::to_string(n1) + ")", "Test 516: encastrement — 1 entrée, 1 notification");
        const std::string fp1 = fingerprint(m);
        w.setNodes(n1, n2);
        choose(TSA::Model::SupportType::Fixed);
        TEST_CHECK(p.undoDelta() == 1, "Test 516: même appui → aucun effet (MemberEndNodesWidget.cpp:139)");
        choose(TSA::Model::SupportType::Pinned); // clé différente : jamais fondue, quel que soit le délai
        TEST_CHECK(p.undoDelta() == 2, "Test 516: autre appui → clé différente (libellé du type), nouvelle entrée");
        TEST_CHECK(m.undo() && fingerprint(m) == fp1 && m.undo() && fingerprint(m) == fp0, "Test 516: Annuler ×2 rend l'état initial");
        std::cout << "[PASS] Test 516: F3 appui depuis les nœuds d'extrémité" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 517 : F4 — grilles (GridDialog.cpp:1344, GridSettingsDialog.cpp:395 et 474). Aucune notification d'observateur.
    // -----------------------------------------------------------------------------------------------------------------
    {
        ModalAutoAnswer modal;
        Model m;
        TSA::Grid::GridManager gm;
        UndoChar::prepareModel(m, &gm);
        // NB (CONSTATÉ) : GridManager crée une grille par défaut (GridManager.cpp:15-22) et GridDefinition(nom, Cartésien)
        // porte des axes par défaut (GridDefinition.cpp:45-56). Le repère d'un Model neuf est vide (Model.cpp:16-17) : valider
        // la grille active y recopie axes et niveaux (GridDialog.cpp:1392-1399).
        TEST_CHECK(gm.grids().size() == 1, "Test 517: grille par défaut du GridManager");
        // Définition sans ligne : onApply sort AVANT le push (GridDialog.cpp:1328-1340 ; sender() nul → pas de boîte).
        {
            TSA::Grid::GridDefinition empty("Vide", TSA::Grid::GridType::Cartesian);
            empty.setXAxes(std::vector<double>{}, std::vector<std::string>{});
            empty.setYAxes(std::vector<double>{}, std::vector<std::string>{});
            TSA::UI::GridDialog dlg(empty, &gm, &m);
            UndoProbe p(m);
            QMetaObject::invokeMethod(&dlg, "onApply", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 0 && gm.grids().size() == 1 && p.obs.events.empty(), "Test 517: grille sans ligne → aucun effet (entrées "
                       << p.undoDelta() << ", grilles " << gm.grids().size() << ")");
        }
        // Modification de la grille active (mode édition, GridDialog.cpp:96-101) : 1 entrée même si rien ne change.
        {
            TSA::UI::GridDialog dlg(&gm, &m);
            const std::string fp0 = fingerprint(m, &gm);
            UndoProbe p(m);
            QMetaObject::invokeMethod(&dlg, "onApply", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 1 && gm.grids().size() == 1, "Test 517: grille active validée, 1 entrée");
            TEST_CHECK(p.obs.specificSequence().empty(), "Test 517: aucune notification d'observateur hors ModelEdited (" << p.obs.specificSequence() << ")");
            std::cout << "  [INFO] Test 517: validation de la grille active " << (fingerprint(m, &gm) == fp0 ? "sans" : "avec")
                      << " changement d'état (" << firstDifference(fp0, fingerprint(m, &gm)) << ")" << std::endl;
        }
        // Création (définition dont l'id est absent du gestionnaire → addGrid, GridDialog.cpp:1365-1375).
        {
            TSA::Grid::GridDefinition def("Grille test", TSA::Grid::GridType::Cartesian);
            def.setXAxes({ 0.0, 6.0 }, { "1", "2" });
            def.setYAxes({ 0.0, 5.0 }, { "A", "B" });
            TSA::UI::GridDialog dlg(def, &gm, &m);
            const std::string fp0 = fingerprint(m, &gm);
            UndoProbe p(m);
            QMetaObject::invokeMethod(&dlg, "onApply", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 1 && gm.grids().size() == 2, "Test 517: grille créée, 1 entrée");
            TEST_CHECK(p.obs.specificSequence().empty(), "Test 517: création — aucune notification d'observateur hors ModelEdited (" << p.obs.specificSequence() << ")");
            const std::string fp1 = fingerprint(m, &gm);
            TEST_CHECK(fp1 != fp0, "Test 517: empreinte modifiée (grilles)");
            if (!undoRedoRoundTrip(m, &gm, fp0, fp1, "Test 517: création de grille")) return false;
        }
        // GridSettingsDialog : le push exige une vue 3D portant le modèle (395, 474). OccView non affichée : constructeur et
        // setModel sans OpenGL (OccView.cpp:75-113, 397-416), rebuildGrid sort si le contexte est nul (OccView_Navigation.cpp:2435).
        {
            OccView view;
            view.setModel(&m);
            TSA::UI::GridSettingsDialog settings(&gm, nullptr, &view);
            auto* list = settings.findChild<QListWidget*>();
            TEST_CHECK(list && list->count() == 2, "Test 517: liste des grilles");
            list->setCurrentRow(0);
            m.clearUndoRedo();
            UndoProbe p(m);
            QMetaObject::invokeMethod(&settings, "onDuplicateGrid", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 1 && gm.grids().size() == 3 && p.obs.specificSequence().empty(), "Test 517: duplication — 1 entrée, aucune notification");
            list->setCurrentRow(2);
            modal.answer = QMessageBox::Yes;
            QMetaObject::invokeMethod(&settings, "onDeleteGrid", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 2 && gm.grids().size() == 2, "Test 517: suppression — 1 entrée");
            TEST_CHECK(m.undo() && gm.grids().size() == 3 && m.undo() && gm.grids().size() == 2, "Test 517: Annuler suppression puis duplication");
            view.setModel(nullptr);
        }
        {
            // Dernière grille : refus avant le push (GridSettingsDialog.cpp:464-468).
            Model m2;
            TSA::Grid::GridManager gm2;
            UndoChar::prepareModel(m2, &gm2);
            OccView view;
            view.setModel(&m2);
            TSA::UI::GridSettingsDialog settings(&gm2, nullptr, &view);
            settings.findChild<QListWidget*>()->setCurrentRow(0);
            UndoProbe p(m2);
            QMetaObject::invokeMethod(&settings, "onDeleteGrid", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 0 && gm2.grids().size() == 1, "Test 517: dernière grille non supprimée, aucune entrée");
            view.setModel(nullptr);
        }
        {
            // Sans vue 3D : duplication HORS historique (DÉFAUT FIGÉ : GridSettingsDialog.cpp:394-396).
            TSA::UI::GridSettingsDialog settings(&gm, nullptr, nullptr);
            settings.findChild<QListWidget*>()->setCurrentRow(0);
            const size_t before = gm.grids().size();
            UndoProbe p(m);
            QMetaObject::invokeMethod(&settings, "onDuplicateGrid", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 0 && gm.grids().size() == before + 1, "Test 517: sans vue — grille dupliquée sans entrée Annuler");
        }
        std::cout << "[PASS] Test 517: F4 grilles (création, duplication, suppression)" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 518 : F5 — création de nœud (NewNodeDialog.cpp:182) et insertion de bibliothèque (LibraryManager.cpp:329).
    // -----------------------------------------------------------------------------------------------------------------
    {
        ModalAutoAnswer modal;
        Model m;
        UndoChar::prepareModel(m);
        {
            TSA::UI::NewNodeDialog dlg(&m);
            dlg.setInitialCoordinates(2.0, 3.0, 0.0);
            for (auto* c : dlg.findChildren<QComboBox*>())
                if (c->findData(static_cast<int>(TSA::Model::SupportType::Fixed)) >= 0) c->setCurrentIndex(c->findData(static_cast<int>(TSA::Model::SupportType::Fixed)));
            const std::string fp0 = fingerprint(m);
            UndoProbe p(m);
            QMetaObject::invokeMethod(&dlg, "onCreateClicked", Qt::DirectConnection);
            const int id = dlg.createdNodeId();
            TEST_CHECK(p.undoDelta() == 1 && id > 0 && m.getNode(id)->supportType() == TSA::Model::SupportType::Fixed, "Test 518: nœud créé encastré");
            TEST_CHECK(p.obs.specificSequence() == "NodeAdded(" + std::to_string(id) + ")",
                       "Test 518: NodeAdded seul — appui posé sans NodeModified (DÉFAUT FIGÉ, NewNodeDialog.cpp:186-191)");
            const std::string fp1 = fingerprint(m);
            if (!undoRedoRoundTrip(m, nullptr, fp0, fp1, "Test 518: création de nœud")) return false;
        }
        {
            // Position occupée, réponse « Oui » (sélectionner l'existant) : aucune entrée.
            TSA::UI::NewNodeDialog dlg(&m);
            dlg.setInitialCoordinates(2.0, 3.0, 0.0);
            modal.answer = QMessageBox::Yes;
            UndoProbe p(m);
            QMetaObject::invokeMethod(&dlg, "onCreateClicked", Qt::DirectConnection);
            TEST_CHECK(p.undoDelta() == 0 && dlg.createdNodeId() > 0, "Test 518: nœud existant sélectionné, aucune entrée");
        }
        {
            // Bibliothèque : écrit dans le dossier utilisateur (comme le test 23) ; nom unique, retiré en fin de test.
            auto& lib = TSA::Library::LibraryManager::instance();
            Model src;
            const int a = src.addNode(0, 0, 0), c = src.addNode(0, 0, 3);
            src.addBeam(a, c);
            src.addWall(a, c);   // voile du gabarit : non instancié (LibraryManager.cpp:331-374)
            const QString name = QStringLiteral("UndoChar_518");
            // Retrait garanti des gabarits du test, même si une vérification échoue (TEST_CHECK sort de la fonction)
            struct TemplateCleanup {
                TSA::Library::LibraryManager& lib;
                ~TemplateCleanup() {
                    lib.removeStructureTemplate(QStringLiteral("UndoChar_518"));
                    lib.removeStructureTemplate(QStringLiteral("UndoChar_518_vide"));
                }
            } cleanup{ lib };
            lib.removeStructureTemplate(name);   // reste d'une exécution interrompue
            lib.removeStructureTemplate(QStringLiteral("UndoChar_518_vide"));
            lib.addStructureTemplate(name, "Tests", "caractérisation", src.createSnapshot("gabarit"));
            UndoProbe p(m);
            TEST_CHECK(!lib.instantiateTemplateInModel("absent_518", &m) && p.undoDelta() == 0, "Test 518: gabarit inconnu → aucun effet");
            const std::string fp0 = fingerprint(m);
            TEST_CHECK(lib.instantiateTemplateInModel(name, &m, 10, 0, 0) && p.undoDelta() == 1, "Test 518: insertion — 1 entrée");
            TEST_CHECK(p.obs.count("NodeAdded") == 2 && p.obs.count("BeamAdded") == 1 && p.obs.count("WallAdded") == 0,
                       "Test 518: nœuds et barre notifiés, voile ignoré (" << p.obs.specificSequence() << ")");
            const std::string fp1 = fingerprint(m);
            if (!undoRedoRoundTrip(m, nullptr, fp0, fp1, "Test 518: insertion de gabarit")) return false;
            Model empty;
            lib.addStructureTemplate(QStringLiteral("UndoChar_518_vide"), "Tests", "", empty.createSnapshot("vide"));
            UndoProbe p2(m);
            TEST_CHECK(lib.instantiateTemplateInModel(QStringLiteral("UndoChar_518_vide"), &m) && p2.undoDelta() == 1 && p2.obs.specificSequence().empty(),
                       "Test 518: gabarit vide → entrée sans changement (DÉFAUT FIGÉ)");
        }
        std::cout << "[PASS] Test 518: F5 création de nœud et insertion de bibliothèque" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 519 : F6 — application d'une proposition IA (AIToolRegistry.cpp:392).
    // -----------------------------------------------------------------------------------------------------------------
    {
        Model m;
        UndoChar::prepareModel(m);
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0), c = m.addNode(10, 0, 0);
        const int b1 = m.addBar(a, b, Section::ipe(200), Material::steelS235(), BarRole::Beam);
        const int b2 = m.addBar(b, c, Section::ipe(200), Material::steelS235(), BarRole::Beam);
        m.clearUndoRedo();
        auto proposal = [](const QString& section, const QString& type, const QJsonArray& ids) {
            TSA::AI::ActionProposal p;
            p.tool = QStringLiteral("propose_section_change");
            p.arguments = QJsonObject{ { "section", section }, { "type", type }, { "ids", ids } };
            return p;
        };
        QString err;
        UndoProbe p(m);
        TEST_CHECK(!TSA::AI::AIToolRegistry::applyProposal(proposal("IPE 999", "beam", { b1 }), m, &err) && p.undoDelta() == 0,
                   "Test 519: section inconnue → aucun effet");
        TEST_CHECK(!TSA::AI::AIToolRegistry::applyProposal(proposal("IPE 300", "beam", { b1, 777 }), m, &err) && p.undoDelta() == 0 && p.obs.events.empty(),
                   "Test 519: élément introuvable → aucun effet (contrôle avant le push)");
        const std::string fp0 = fingerprint(m);
        TEST_CHECK(TSA::AI::AIToolRegistry::applyProposal(proposal("IPE 300", "beam", { b1, b2 }), m, &err) && p.undoDelta() == 1,
                   "Test 519: proposition appliquée, 1 entrée");
        TEST_CHECK(p.obs.specificSequence() == "BeamModified(" + std::to_string(b1) + ") BeamModified(" + std::to_string(b2) + ")",
                   "Test 519: une notification par élément, dans l'ordre des ids");
        const std::string fp1 = fingerprint(m);
        if (!undoRedoRoundTrip(m, nullptr, fp0, fp1, "Test 519: proposition IA")) return false;
        UndoProbe p2(m);
        TEST_CHECK(TSA::AI::AIToolRegistry::applyProposal(proposal("IPE 300", "beam", {}), m, &err) && p2.undoDelta() == 1 && p2.obs.specificSequence().empty(),
                   "Test 519: liste d'ids vide → entrée sans changement (DÉFAUT FIGÉ)");
        std::cout << "[PASS] Test 519: F6 proposition IA" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 520 : révision, ModelEdited, EditRecord et transitions Annuler / Rétablir — une opération représentative par
    // famille (F1–F6), puis les catégories de transaction : validée avec modification, sans modification, annulée,
    // échouée, refusée avant écriture, coalescée. Contrat : undo_characterization.h §5 et checkTransition.
    // Incréments de révision figés (CONSTATÉS le 2026-10-26, expliqués par le code) : pushUndoState ou commit +1
    // (setModified → bumpRevision, Model.h:414-418) ; chaque notification spécifique +1 (une paire charge spécifique +
    // générique = +1, Model.cpp:1211-1219) ; mutations par accesseurs bruts dans une transaction : 0 ; grilles : aucune
    // notification ; rollback +1 (notifyModelDiffApplied), même sans modification (DÉFAUT FIGÉ) ; refus avant écriture : 0.
    // -----------------------------------------------------------------------------------------------------------------
    {
        ModalAutoAnswer modal;
        Model m;
        TSA::Grid::GridManager gm;
        UndoChar::prepareModel(m, &gm);
        using UndoChar::measureTransition;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(6, 0, 0), n4 = m.addNode(0, 4, 0), n5 = m.addNode(0, 0, 3);
        const int beam = m.addBar(n1, n2, Section::rectangular(0.25, 0.55), Material::steelS355(), BarRole::Beam, 12.5, "B_520");
        m.addColumn(n1, n5, Section::rectangular(0.35, 0.45), Material::concreteC25_30(), 0.0, "C_520");
        m.clearUndoRedo();

        // F1 — poutre, cran de rotation (BeamPropertiesView.cpp:424 ; valueChanged → applyChanges).
        {
            TSA::UI::BeamPropertiesView v(&m);
            v.setElementId(beam);
            QDoubleSpinBox* rot = spinWithValue(v, 12.5);
            TEST_CHECK(rot != nullptr, "Test 520: champ rotation de la poutre trouvé");
            const auto t = measureTransition(m, &gm, [&] { rot->setValue(20.0); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 520: F1 poutre (rotation)")) return false;
        }
        // F1 — nœud, coordonnée Y (NodePropertiesView.cpp:522) : EditRecord « coordinates » (NodePropertiesView.cpp:537).
        {
            TSA::UI::NodePropertiesView v(&m);
            v.setElementId(n4);
            QDoubleSpinBox* y = spinWithValue(v, 4.0);
            TEST_CHECK(y != nullptr, "Test 520: champ Y du nœud trouvé");
            const auto t = measureTransition(m, &gm, [&] { y->setValue(4.5); });
            if (!checkTransition(t, 1, 2, 1, "coordinates", "Test 520: F1 nœud (coordonnée)")) return false;
        }
        // F2 — ajout de cas de charge (LoadCaseDialog.cpp:214).
        {
            TSA::UI::LoadCaseDialog dlg(&m);
            const auto t = measureTransition(m, &gm, [&] { QMetaObject::invokeMethod(&dlg, "onAddLoadCase", Qt::DirectConnection); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 520: F2 cas de charge")) return false;
        }
        // F2 — charge sur barre (MemberLoadCommands.cpp:80).
        {
            const MemberLoad proto = MemberLoad::uniform(0, 0, 1, -4.0, LoadDirection::Gravity);
            std::string err;
            const auto t = measureTransition(m, &gm, [&] { TSA::Model::applyMemberLoad(m, proto, { { beam } }, "charge 520", &err); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 520: F2 charge sur barre")) return false;
        }
        // F3 — appui depuis les nœuds d'extrémité (MemberEndNodesWidget.cpp:143).
        {
            TSA::UI::MemberEndNodesWidget w;
            w.setModel(&m);
            w.setNodes(n1, n5);
            QComboBox* start = w.findChildren<QComboBox*>().value(0);
            TEST_CHECK(start && start->isEnabled(), "Test 520: liste des appuis");
            const int i = start->findData(static_cast<int>(TSA::Model::SupportType::Fixed));
            const auto t = measureTransition(m, &gm, [&] { start->setCurrentIndex(i); emit start->activated(i); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 520: F3 appui")) return false;
        }
        // F4 — création de grille (GridDialog.cpp:1344) : aucune notification d'observateur, état porté par les grilles.
        {
            TSA::Grid::GridDefinition def("Grille 520", TSA::Grid::GridType::Cartesian);
            def.setXAxes({ 0.0, 6.0 }, { "1", "2" });
            def.setYAxes({ 0.0, 4.0 }, { "A", "B" });
            TSA::UI::GridDialog dlg(def, &gm, &m);
            const auto t = measureTransition(m, &gm, [&] { QMetaObject::invokeMethod(&dlg, "onApply", Qt::DirectConnection); });
            if (!checkTransition(t, 1, 1, 0, "", "Test 520: F4 grille")) return false;
            TEST_CHECK(t.specificOp.empty() && t.fOp != t.f0, "Test 520: F4 — état modifié sans notification spécifique (" << t.specificOp << ")");
        }
        // F5 — création de nœud (NewNodeDialog.cpp:182).
        {
            TSA::UI::NewNodeDialog dlg(&m);
            dlg.setInitialCoordinates(9.0, 9.0, 0.0);
            const auto t = measureTransition(m, &gm, [&] { QMetaObject::invokeMethod(&dlg, "onCreateClicked", Qt::DirectConnection); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 520: F5 nœud")) return false;
        }
        // F6 — proposition IA (AIToolRegistry.cpp:392).
        {
            TSA::AI::ActionProposal prop;
            prop.tool = QStringLiteral("propose_section_change");
            prop.arguments = QJsonObject{ { "section", "IPE 300" }, { "type", "beam" }, { "ids", QJsonArray{ beam } } };
            QString err;
            const auto t = measureTransition(m, &gm, [&] { TSA::AI::AIToolRegistry::applyProposal(prop, m, &err); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 520: F6 proposition IA")) return false;
        }

        // Catégories de transaction (EditTransaction ; UndoManager.cpp:166-200, Model_Snapshots.cpp:172).
        using TSA::UndoRedo::EditTransaction;
        {
            const auto t = measureTransition(m, &gm, [&] {
                EditTransaction tx(m, "validée");
                m.getBeam(beam)->setRotation(33.0);
                tx.commit();
            });
            if (!checkTransition(t, 1, 1, 0, "", "Test 520: transaction validée avec modification")) return false;
            TEST_CHECK(t.fOp != t.f0, "Test 520: transaction validée — état modifié");
        }
        {
            const auto t = measureTransition(m, &gm, [&] {
                EditTransaction tx(m, "sans modification");
                tx.commit();
            });
            if (!checkTransition(t, 0, 0, 0, "", "Test 520: transaction validée sans modification (BUG-127 : aucune entrée, révision inchangée)")) return false;
            TEST_CHECK(t.fOp == t.f0, "Test 520: transaction sans modification — état inchangé");
        }
        {
            const auto t = measureTransition(m, &gm, [&] {
                EditTransaction tx(m, "annulée");
                tx.rollback();
            });
            if (!checkTransition(t, 0, 0, 0, "", "Test 520: transaction annulée sans modification (BUG-127 : révision inchangée)")) return false;
            TEST_CHECK(t.fOp == t.f0, "Test 520: transaction annulée — état inchangé");
        }
        {
            const auto t = measureTransition(m, &gm, [&] {
                try
                {
                    EditTransaction tx(m, "échouée");
                    m.getBeam(beam)->setRotation(44.0);
                    throw std::runtime_error("échec");
                }
                catch (const std::runtime_error&) {}
            });
            if (!checkTransition(t, 0, 1, 0, "", "Test 520: transaction échouée après modification (destructeur → rollback)")) return false;
            TEST_CHECK(t.fOp == t.f0, "Test 520: transaction échouée — état restauré");
        }
        {
            // Refus avant toute écriture : n1 déplacé sur n2 (NodePropertiesView.cpp:510-517).
            TSA::UI::NodePropertiesView v(&m);
            v.setElementId(n1);
            QDoubleSpinBox* x = spinWithRange(v, -100000.0, 100000.0);
            TEST_CHECK(x != nullptr, "Test 520: champ X du nœud trouvé");
            const auto t = measureTransition(m, &gm, [&] { x->setValue(6.0); });
            if (!checkTransition(t, 0, 0, 0, "", "Test 520: modification refusée avant écriture")) return false;
            TEST_CHECK(t.fOp == t.f0, "Test 520: refus — état inchangé");
        }
        {
            // Coalescence : deux crans sur la même poutre fondus en une entrée ; Annuler défait les deux (garde temporelle).
            TSA::UI::BeamPropertiesView v(&m);
            v.setElementId(beam);
            QDoubleSpinBox* rot = spinWithValue(v, 33.0);
            TEST_CHECK(rot != nullptr, "Test 520: champ rotation (33°) trouvé");
            CoalesceClock clock;
            const auto t = measureTransition(m, &gm, [&] { rot->setValue(36.0); rot->setValue(39.0); });
            if (clock.safelyInside())
            {
                if (!checkTransition(t, 1, 4, 0, "", "Test 520: deux crans coalescés")) return false;
                TEST_CHECK(t.editedOp >= 2, "Test 520: coalescence — révision incrémentée à chaque cran malgré une seule entrée");
            }
            else
                std::cout << "[SKIP] Test 520: coalescence non vérifiable (intervalle ≥ 1,5 s)" << std::endl;
        }
        std::cout << "[PASS] Test 520: révision, EditRecord et Annuler / Rétablir par famille et par catégorie de transaction" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 521 : allers-retours Annuler / Rétablir par les chemins de production — treillis et câbles (création par
    // ElementCreation, modification par les panneaux, suppression par CommandManager + DeleteElementsCommand comme
    // MainWindow.cpp:926-940), charges nodales (modification, retrait), cas de charge (suppression), coalescence.
    // -----------------------------------------------------------------------------------------------------------------
    {
        ModalAutoAnswer modal;
        Model m;
        UndoChar::prepareModel(m);
        using UndoChar::measureTransition;
        TSA::Interaction::ElementCreation creation(m);
        TSA::UndoRedo::CommandManager commands(&m, m.undoManager()); // une transaction par commande (CommandManager.cpp:25-55)
        auto pt = [](double x, double y, double z, int node = -1) { return TSA::Interaction::DrawPoint(gp_Pnt(x, y, z), node); };
        auto deleteElements = [&](std::set<int> trusses, std::set<int> cables) {
            return commands.executeCommand(std::make_unique<TSA::Commands::DeleteElementsCommand>(
                m, std::set<int>{}, std::set<int>{}, std::set<int>{}, std::set<int>{}, std::set<int>{}, std::set<int>{}, trusses, cables));
        };

        // Treillis : création (ElementCreation.cpp:395-430), modification du nom (TrussMemberPropertiesView.cpp:174),
        // diamètre en deux crans coalescés, suppression.
        int truss = -1;
        {
            TSA::Model::TrussPreset tp;
            const auto t = measureTransition(m, nullptr, [&] {
                truss = creation.createTrussMember(pt(0, 0, 0), pt(4, 0, 0), tp,
                                                   [](Model& model, int id) { model.getTrussMember(id)->setName("T_521"); }).elementId;
            });
            if (!checkTransition(t, 1, 4, 0, "", "Test 521: création de treillis")) return false;
            TEST_CHECK(truss > 0 && t.specificOp.find("TrussMemberAdded(" + std::to_string(truss) + ")") != std::string::npos,
                       "Test 521: TrussMemberAdded notifié (" << t.specificOp << ")");
        }
        {
            TSA::UI::TrussMemberPropertiesView v(&m);
            v.setElementId(truss);
            QLineEdit* name = editWithText(v, "T_521");
            TEST_CHECK(name != nullptr, "Test 521: champ du nom du treillis trouvé");
            const auto t = measureTransition(m, nullptr, [&] { name->setText("T_521_mod"); v.applyChanges(); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 521: modification de treillis")) return false;
            TEST_CHECK(m.getTrussMember(truss)->name() == "T_521_mod", "Test 521: nom du treillis rétabli après Rétablir");
        }
        {
            TSA::UI::TrussMemberPropertiesView v(&m);
            v.setElementId(truss);
            QDoubleSpinBox* diameter = spinWithRange(v, 0.005, 1.0);
            TEST_CHECK(diameter != nullptr, "Test 521: champ diamètre du treillis trouvé");
            CoalesceClock clock;
            const auto t = measureTransition(m, nullptr, [&] { diameter->setValue(0.12); diameter->setValue(0.14); });
            if (clock.safelyInside())
            {
                if (!checkTransition(t, 1, 4, 0, "", "Test 521: diamètre du treillis, deux crans coalescés")) return false;
            }
            else
                std::cout << "[SKIP] Test 521: coalescence du treillis non vérifiable (intervalle ≥ 1,5 s)" << std::endl;
        }
        {
            bool executed = false;
            const auto t = measureTransition(m, nullptr, [&] { executed = deleteElements({ truss }, {}); });
            TEST_CHECK(executed, "Test 521: suppression du treillis exécutée");
            if (!checkTransition(t, 1, 2, 0, "", "Test 521: suppression de treillis")) return false;
            TEST_CHECK(!m.getTrussMember(truss) && t.specificOp == "TrussMemberRemoved(" + std::to_string(truss) + ")",
                       "Test 521: TrussMemberRemoved seul (" << t.specificOp << ")");
        }

        // Câble : création (ElementCreation.cpp:433-458), modification du nom (CablePropertiesView.cpp:269 ; l'aire est
        // réécrite depuis l'affichage arrondi, DÉFAUT FIGÉ au test 514), flèche en deux crans coalescés, suppression.
        int cable = -1;
        {
            const auto t = measureTransition(m, nullptr, [&] {
                cable = creation.createCable(pt(0, 4, 0), pt(8, 4, 0), TSA::Model::CableType::StayCable,
                                             [](Model& model, int id) { model.getCable(id)->setName("K_521"); }).elementId;
            });
            if (!checkTransition(t, 1, 4, 0, "", "Test 521: création de câble")) return false;
            TEST_CHECK(cable > 0 && t.specificOp.find("CableAdded(" + std::to_string(cable) + ")") != std::string::npos,
                       "Test 521: CableAdded notifié (" << t.specificOp << ")");
        }
        {
            TSA::UI::CablePropertiesView v(&m);
            v.setElementId(cable);
            QLineEdit* name = editWithText(v, "K_521");
            TEST_CHECK(name != nullptr, "Test 521: champ du nom du câble trouvé");
            const auto t = measureTransition(m, nullptr, [&] { name->setText("K_521_mod"); v.applyChanges(); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 521: modification de câble")) return false;
            TEST_CHECK(m.getCable(cable)->name() == "K_521_mod", "Test 521: nom du câble rétabli après Rétablir");
        }
        {
            TSA::UI::CablePropertiesView v(&m);
            v.setElementId(cable);
            QDoubleSpinBox* sag = spinWithRange(v, 0.0, 100.0);
            TEST_CHECK(sag != nullptr, "Test 521: champ flèche du câble trouvé");
            CoalesceClock clock;
            const auto t = measureTransition(m, nullptr, [&] { sag->setValue(sag->value() + 0.5); sag->setValue(sag->value() + 0.5); });
            if (clock.safelyInside())
            {
                if (!checkTransition(t, 1, 4, 0, "", "Test 521: flèche du câble, deux crans coalescés")) return false;
            }
            else
                std::cout << "[SKIP] Test 521: coalescence du câble non vérifiable (intervalle ≥ 1,5 s)" << std::endl;
        }
        {
            bool executed = false;
            const auto t = measureTransition(m, nullptr, [&] { executed = deleteElements({}, { cable }); });
            TEST_CHECK(executed, "Test 521: suppression du câble exécutée");
            if (!checkTransition(t, 1, 2, 0, "", "Test 521: suppression de câble")) return false;
            TEST_CHECK(!m.getCable(cable) && t.specificOp == "CableRemoved(" + std::to_string(cable) + ")",
                       "Test 521: CableRemoved seul (" << t.specificOp << ")");
        }

        // Charge nodale : modification par le panneau (LoadPropertiesView.cpp:339), retrait (MemberLoadCommands.cpp:107).
        {
            const int node = m.nodes().begin()->first;
            const int nl = m.loadManager().addNodalLoad(NodalLoad(0, node, 1, 0, 0, -10, 0, 0, 0, LoadCoordSystem::Global, "NL_521"));
            m.clearUndoRedo();
            TSA::UI::LoadPropertiesView v(&m);
            v.setNodalLoadId(nl);
            QDoubleSpinBox* fz = spinWithValue(v, -10.0);
            TEST_CHECK(fz != nullptr, "Test 521: champ Fz de la charge nodale trouvé");
            const auto t = measureTransition(m, nullptr, [&] { fz->setValue(-12.0); v.applyChanges(); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 521: modification de charge nodale")) return false;
            TEST_CHECK(t.specificOp == "NodalLoadModified(" + std::to_string(nl) + ") LoadModified(" + std::to_string(nl) + ")",
                       "Test 521: NodalLoadModified + LoadModified (" << t.specificOp << ")");
            const auto t2 = measureTransition(m, nullptr, [&] { TSA::Model::removeLoads(m, { nl }, {}, "retrait NL_521"); });
            if (!checkTransition(t2, 1, 2, 0, "", "Test 521: retrait de charge nodale")) return false;
            TEST_CHECK(t2.specificOp == "NodalLoadRemoved(" + std::to_string(nl) + ") LoadRemoved(" + std::to_string(nl) + ")",
                       "Test 521: NodalLoadRemoved + LoadRemoved (" << t2.specificOp << ")");
        }
        // Cas de charge : suppression (LoadCaseDialog.cpp:231), Annuler puis Rétablir.
        {
            TSA::UI::LoadCaseDialog dlg(&m);
            TEST_CHECK(selectRowWithText(dlg, QStringLiteral("Q")), "Test 521: ligne du cas Q sélectionnée");
            modal.answer = QMessageBox::Yes;
            const auto t = measureTransition(m, nullptr, [&] { QMetaObject::invokeMethod(&dlg, "onRemoveLoadCase", Qt::DirectConnection); });
            if (!checkTransition(t, 1, 2, 0, "", "Test 521: suppression de cas de charge")) return false;
        }
        std::cout << "[PASS] Test 521: allers-retours treillis, câbles, charges nodales, cas de charge, coalescence" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 522 : dialogues de grilles — GridDialog (grille active : recopie explicite dans le repère du modèle) et
    // GridSettingsDialog (empreinte avant / après duplication et suppression, Annuler / Rétablir).
    // -----------------------------------------------------------------------------------------------------------------
    {
        ModalAutoAnswer modal;
        Model m;
        TSA::Grid::GridManager gm;
        UndoChar::prepareModel(m, &gm);
        using UndoChar::measureTransition;
        // GridDialog, grille active (GridDialog.cpp:1376-1400 → GridCoordinateSync.cpp:31-56) : axes X/Y (décalés de
        // l'origine) et niveaux recopiés dans le repère ; rien d'autre ne change.
        {
            const TSA::Grid::GridDefinition def = gm.activeGrid()->definition(); // copie : la validation met la grille à jour
            auto csJson = [&m] { return QJsonDocument::fromJson(QByteArray::fromStdString(m.coordinateSystem()->serializeToJson())).object(); };
            auto labels = [](const QJsonValue& v) {
                std::vector<std::string> out;
                for (const auto& x : v.toArray()) out.push_back(x.toString().toStdString());
                return out;
            };
            auto positions = [](const QJsonValue& v) {
                std::vector<double> out;
                for (const auto& x : v.toArray()) out.push_back(x.toDouble());
                return out;
            };
            auto shifted = [](std::vector<double> v, double o) { for (double& x : v) x += o; return v; };
            const QJsonObject before = csJson();
            TEST_CHECK(labels(before["xLabels"]).empty() && labels(before["yLabels"]).empty() && before["levels"].toArray().isEmpty(),
                       "Test 522: repère d'un modèle neuf vide (Model.cpp:16-17)");
            TSA::UI::GridDialog dlg(&gm, &m);
            const std::string gridsBefore = gm.serializeToJson();
            const auto t = measureTransition(m, &gm, [&] { QMetaObject::invokeMethod(&dlg, "onApply", Qt::DirectConnection); });
            const std::string gridsAfter = gm.serializeToJson(); // après Rétablir = état obtenu (vérifié par checkTransition)
            if (!checkTransition(t, 1, 1, 0, "", "Test 522: validation de la grille active")) return false;
            const std::string noCoords0 = withoutLines(t.f0, "coords|"), noCoords1 = withoutLines(t.fOp, "coords|");
            TEST_CHECK(t.fOp != t.f0 && sameExceptPrefix(noCoords0, noCoords1, "grids|"),
                       "Test 522: seuls le repère et les grilles changent (" << firstDifferenceExcept(noCoords0, noCoords1, "grids|") << ")");
            // CONSTATÉ : la validation sans changement normalise xIsBold / yIsBold / zIsBold (vides → false par axe) ;
            // sans effet visible (aucun axe en gras), le reste de la définition est inchangé.
            auto gridsJson = [](const std::string& json) {
                return QJsonDocument::fromJson(QByteArray::fromStdString(json)).object()["grids"].toArray();
            };
            const QJsonArray g0 = gridsJson(gridsBefore), g1 = gridsJson(gridsAfter);
            TEST_CHECK(g0.size() == 1 && g1.size() == 1, "Test 522: une grille avant et après");
            QJsonObject s0 = g0[0].toObject(), s1 = g1[0].toObject();
            for (const char* k : { "xIsBold", "yIsBold", "zIsBold" })
            {
                s0.remove(QLatin1String(k));
                s1.remove(QLatin1String(k));
            }
            TEST_CHECK(s0 == s1, "Test 522: définition de la grille inchangée hors *IsBold");
            auto allFalse = [](const QJsonValue& v, size_t n) {
                const QJsonArray arr = v.toArray();
                if (static_cast<size_t>(arr.size()) != n) return false;
                for (const auto& x : arr)
                    if (x.toBool()) return false;
                return true;
            };
            TEST_CHECK(g0[0].toObject()["xIsBold"].toArray().isEmpty() && allFalse(g1[0].toObject()["xIsBold"], def.xPositions().size()) &&
                           allFalse(g1[0].toObject()["yIsBold"], def.yPositions().size()) && allFalse(g1[0].toObject()["zIsBold"], def.zLevels().size()),
                       "Test 522: *IsBold normalisés à false, un par axe (comportement caractérisé)");
            const QJsonObject after = csJson();
            TEST_CHECK(labels(after["xLabels"]) == def.xLabels() && labels(after["yLabels"]) == def.yLabels(),
                       "Test 522: libellés X / Y de la grille recopiés dans le repère");
            TEST_CHECK(positions(after["xPositions"]) == shifted(def.xPositions(), def.origin().X()) &&
                           positions(after["yPositions"]) == shifted(def.yPositions(), def.origin().Y()),
                       "Test 522: positions X / Y recopiées (décalées de l'origine)");
            TEST_CHECK(static_cast<size_t>(after["levels"].toArray().size()) == def.zLevels().size(),
                       "Test 522: un niveau par altitude de la grille (" << after["levels"].toArray().size() << " / " << def.zLevels().size() << ")");
            TEST_CHECK(lineWithPrefix(t.fRedo, "coords|") == lineWithPrefix(t.fOp, "coords|"), "Test 522: Rétablir rend le repère recopié");
        }
        // GridSettingsDialog (395, 474) avec une vue 3D non affichée : empreinte à chaque étape.
        {
            OccView view;
            view.setModel(&m);
            TSA::UI::GridSettingsDialog settings(&gm, nullptr, &view);
            auto* list = settings.findChild<QListWidget*>();
            TEST_CHECK(list && list->count() == 1, "Test 522: liste des grilles");
            list->setCurrentRow(0);
            m.clearUndoRedo();
            const auto dup = measureTransition(m, &gm, [&] { QMetaObject::invokeMethod(&settings, "onDuplicateGrid", Qt::DirectConnection); });
            if (!checkTransition(dup, 1, 1, 0, "", "Test 522: duplication de grille")) return false;
            TEST_CHECK(gm.grids().size() == 2 && dup.fOp != dup.f0 && sameExceptPrefix(dup.f0, dup.fOp, "grids|"),
                       "Test 522: duplication — seules les grilles changent (hors grilles : " << firstDifferenceExcept(dup.f0, dup.fOp, "grids|") << ")");
            list->setCurrentRow(1);
            modal.answer = QMessageBox::Yes;
            const auto del = measureTransition(m, &gm, [&] { QMetaObject::invokeMethod(&settings, "onDeleteGrid", Qt::DirectConnection); });
            if (!checkTransition(del, 1, 1, 0, "", "Test 522: suppression de grille")) return false;
            TEST_CHECK(gm.grids().size() == 1 && del.fOp == dup.f0,
                       "Test 522: suppression de la copie — empreinte égale à celle d'avant la duplication (" << firstDifference(dup.f0, del.fOp) << ")");
            view.setModel(nullptr);
        }
        std::cout << "[PASS] Test 522: dialogues de grilles (repère recopié, empreintes, Annuler / Rétablir)" << std::endl;
        ++passed;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // TEST 523 : notifications vérifiées POSITIVEMENT — Added / Removed des familles d'éléments (Model.cpp add* /
    // remove*), charges retirées en cascade, DimensionsChanged (DimensionService.cpp ; suppression d'un nœud coté,
    // Model.cpp:360-375), ModelCleared (clear, restoreSnapshot), ModelDestroyed (~Model, Model.cpp:26-32).
    // -----------------------------------------------------------------------------------------------------------------
    {
        Model m;
        UndoChar::prepareModel(m);
        UndoChar::RecordingObserver obs(&m);
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0), c = m.addNode(5, 4, 0), d = m.addNode(0, 4, 0);
        const int e = m.addNode(0, 0, 3), f = m.addNode(5, 0, 3), g = m.addNode(9, 9, 0);
        TEST_CHECK(obs.count("NodeAdded") == 7 && obs.count("ModelEdited") == 7,
                   "Test 523: NodeAdded ×7, une révision chacun (" << obs.fullSequence() << ")");
        obs.clear();
        const int beam = m.addBar(a, b, Section::ipe(200), Material::steelS235(), BarRole::Beam);
        const int col = m.addColumn(a, e);
        const int slab = m.addSlab({ a, b, c, d }, 0.2, "S_523");
        const int wall = m.addWall(e, f, 3.0, 0.2, "W_523");
        const int found = m.addFoundation(c, 1.5, 1.5, 0.6, "F_523");
        const int truss = m.addTrussMember(b, e, 0.1, "T_523");
        const int cable = m.addCable(d, f, 0.02, "K_523");
        auto id = [](int x) { return std::to_string(x); };
        const std::string added = "BeamAdded(" + id(beam) + ") ColumnAdded(" + id(col) + ") SlabAdded(" + id(slab) + ") WallAdded(" + id(wall) +
                                  ") FoundationAdded(" + id(found) + ") TrussMemberAdded(" + id(truss) + ") CableAdded(" + id(cable) + ")";
        TEST_CHECK(obs.specificSequence() == added, "Test 523: Added, une par famille, dans l'ordre (" << obs.specificSequence() << ")");
        TEST_CHECK(obs.count("ModelEdited") == 7, "Test 523: une révision par ajout (" << obs.count("ModelEdited") << ")");

        // Retraits : la charge portée par la poutre est retirée et notifiée AVANT la poutre (Model.cpp:599-620).
        const int ml = m.loadManager().addMemberLoad(MemberLoad::uniform(0, beam, 1, -2.0, LoadDirection::Gravity));
        obs.clear();
        TEST_CHECK(m.removeBeam(beam) && obs.specificSequence() == "MemberLoadRemoved(" + id(ml) + ") LoadRemoved(" + id(ml) + ") BeamRemoved(" + id(beam) + ")",
                   "Test 523: retrait de poutre chargée (" << obs.specificSequence() << ")");
        obs.clear();
        TEST_CHECK(m.removeColumn(col) && m.removeSlab(slab) && m.removeWall(wall) && m.removeFoundation(found) && m.removeTrussMember(truss) &&
                       m.removeCable(cable),
                   "Test 523: retraits effectués");
        TEST_CHECK(obs.specificSequence() == "ColumnRemoved(" + id(col) + ") SlabRemoved(" + id(slab) + ") WallRemoved(" + id(wall) +
                                                 ") FoundationRemoved(" + id(found) + ") TrussMemberRemoved(" + id(truss) + ") CableRemoved(" + id(cable) + ")",
                   "Test 523: Removed, une par famille (" << obs.specificSequence() << ")");
        // Nœud portant une charge nodale : NodalLoadRemoved + LoadRemoved puis NodeRemoved (Model.cpp:340-375).
        const int nl = m.loadManager().addNodalLoad(NodalLoad(0, g, 1, 0, 0, -1));
        obs.clear();
        TEST_CHECK(m.removeNode(g) && obs.specificSequence() == "NodalLoadRemoved(" + id(nl) + ") LoadRemoved(" + id(nl) + ") NodeRemoved(" + id(g) + ")",
                   "Test 523: retrait de nœud chargé (" << obs.specificSequence() << ")");

        // Cotations : DimensionsChanged sans changement de révision (Model.h, notifyDimensionsChanged).
        {
            TSA::Annotation::Dimension dim;
            dim.kind = TSA::Annotation::DimensionKind::Linear;
            dim.axis = TSA::Annotation::MeasureAxis::Aligned;
            TSA::Annotation::DimensionAnchor x, y;
            x.nodeId = a;
            y.nodeId = b;
            dim.anchors = { x, y };
            dim.position = { 2.5, -1.0, 0.0 };
            std::string err;
            const auto rev = m.revision();
            obs.clear();
            const int dimId = TSA::Annotation::addDimension(m, dim, &err);
            TEST_CHECK(dimId > 0 && obs.specificSequence() == "DimensionsChanged[" + id(dimId) + "]" && obs.count("ModelEdited") == 0 && m.revision() == rev,
                       "Test 523: cotation ajoutée — DimensionsChanged, révision inchangée (" << obs.fullSequence() << ", " << err << ")");
            obs.clear();
            TEST_CHECK(TSA::Annotation::removeDimensions(m, { dimId }) == 1 && obs.specificSequence() == "DimensionsChanged[" + id(dimId) + "]",
                       "Test 523: cotation retirée — DimensionsChanged (" << obs.specificSequence() << ")");
            const int dim2 = TSA::Annotation::addDimension(m, dim, &err);
            obs.clear();
            TEST_CHECK(m.removeNode(a) && obs.specificSequence() == "DimensionsChanged[" + id(dim2) + "] NodeRemoved(" + id(a) + ")",
                       "Test 523: nœud coté retiré — cotation orpheline signalée avant NodeRemoved (" << obs.specificSequence() << ")");
        }

        // ModelCleared : clear() (Model_Snapshots.cpp:294-334) puis restoreSnapshot (DeleteElementsCommand::undo sans
        // UndoManager, ModifyCommands.cpp:88-93 → Model_Snapshots.cpp:202-211).
        {
            obs.clear();
            m.clear();
            TEST_CHECK(obs.fullSequence() == "ModelEdited ModelCleared" && m.nodes().empty(),
                       "Test 523: clear — un seul ModelCleared (" << obs.fullSequence() << ")");
            const int p = m.addNode(0, 0, 0), q = m.addNode(3, 0, 0);
            const int bar = m.addBar(p, q, Section::ipe(200), Material::steelS235(), BarRole::Beam);
            TSA::UndoRedo::CommandManager noUndo(&m, nullptr); // historique par commandes (CommandManager.cpp:58-70)
            TEST_CHECK(noUndo.executeCommand(std::make_unique<TSA::Commands::DeleteElementsCommand>(
                           m, std::set<int>{}, std::set<int>{ bar }, std::set<int>{}, std::set<int>{}, std::set<int>{}, std::set<int>{},
                           std::set<int>{}, std::set<int>{})) && !m.getBeam(bar),
                       "Test 523: suppression par commande sans UndoManager");
            obs.clear();
            TEST_CHECK(noUndo.undo() && m.getBeam(bar) && obs.specificSequence() == "ModelCleared",
                       "Test 523: annulation par instantané complet — ModelCleared (" << obs.specificSequence() << ")");
        }

        // ModelDestroyed : l'observateur survit au modèle (contrat Model.h:106-110 : ne plus appeler removeObserver).
        {
            auto doomed = std::make_unique<Model>();
            UndoChar::RecordingObserver watcher(doomed.get());
            doomed->addNode(1, 1, 1);
            watcher.clear();
            doomed.reset();
            TEST_CHECK(watcher.fullSequence() == "ModelDestroyed", "Test 523: ModelDestroyed seul à la destruction (" << watcher.fullSequence() << ")");
        }
        std::cout << "[PASS] Test 523: notifications Added / Removed, DimensionsChanged, ModelCleared, ModelDestroyed" << std::endl;
        ++passed;
    }
    return true;
}
