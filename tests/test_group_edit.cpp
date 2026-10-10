// Suite « groupedit » : édition groupée depuis le panneau Propriétés (test 222).
// Plusieurs nœuds sélectionnés, un appui choisi dans les propriétés : tous les nœuds le reçoivent,
// même quand la vue recrée la forme du nœud modifié (ce qui le resélectionnait et interrompait
// l'édition groupée : seul le nœud affiché recevait l'appui, BUG-070).

#include "test_common.h"

#include "UI/Properties/PropertyPanel.h"
#include "Viewer/SelectionManager.h"

#include <QComboBox>
#include <QApplication>
#include <QTreeWidget>
#include <QWheelEvent>
#include "UI/ModelTree/ModelTreeWidget.h"

bool runSuite_GroupEdit(int& passed)
{
    using TSA::Viewer::SelectionManager;
    using TSA::Viewer::SelectionType;

    // -------------------------------------------------------------------------
    // TEST 222 : appui appliqué à tous les nœuds sélectionnés
    // -------------------------------------------------------------------------
    {
        Model m;
        const int n1 = m.addNode(0, 0, 0);
        const int n2 = m.addNode(6, 0, 0);
        const int n3 = m.addNode(12, 0, 0);

        SelectionManager sm;
        TSA::UI::PropertyPanel panel(&m);
        int singleSelections = 0;
        // Comme SelectionSynchronizer : une sélection simple rouvre le panneau sur un seul nœud.
        QObject::connect(&sm, &SelectionManager::nodeSelected, &panel, [&](int id) {
            ++singleSelections;
            panel.showNodeProperties(id);
        });

        // Comme OccView::updateNodeShape : la forme du nœud modifié est recréée, il est retiré puis
        // remis dans la sélection.
        struct ViewRebuild : TSA::Model::IModelObserver
        {
            SelectionManager* sm = nullptr;
            void onNodeModified(const Node& node) override
            {
                const bool wasSelected = sm->selectedNodes().count(node.id()) > 0;
                sm->unregisterNode(node.id());
                if (wasSelected) sm->restoreSelected(SelectionType::Node, node.id());
            }
        } view;
        view.sm = &sm;
        m.addObserver(&view);

        sm.selectNode(n1);
        sm.toggleElement(SelectionType::Node, n2);
        singleSelections = 0;
        panel.showNodeProperties(n1);
        panel.setMultiSelection(TSA::Model::ElementKind::Node, n1, sm.selectedNodes());
        TEST_CHECK(panel.multiEdit().active() && panel.multiEdit().count() == 2, "Test 222: édition groupée de 2 nœuds");

        QComboBox* preset = nullptr;
        for (auto* combo : panel.findChildren<QComboBox*>())
            if (combo->findData(static_cast<int>(TSA::Model::SupportType::Fixed)) >= 0 &&
                combo->findData(static_cast<int>(TSA::Model::SupportType::Elastic)) >= 0)
                preset = combo;
        TEST_CHECK(preset != nullptr, "Test 222: liste des types d'appui trouvée");
        preset->setCurrentIndex(preset->findData(static_cast<int>(TSA::Model::SupportType::Fixed)));

        TEST_CHECK(m.getNode(n1)->support().isFixed(), "Test 222: nœud principal encastré");
        TEST_CHECK(m.getNode(n2)->support().isFixed(), "Test 222: second nœud sélectionné encastré aussi");
        TEST_CHECK(!m.getNode(n3)->support().isFixed(), "Test 222: nœud non sélectionné inchangé");
        TEST_CHECK(singleSelections == 0 && panel.multiEdit().active(), "Test 222: l'édition groupée reste active");
        TEST_CHECK(sm.selectedNodes() == std::set<int>({ n1, n2 }), "Test 222: sélection conservée");

        // Autre type ensuite (appui simple) : toujours appliqué aux deux nœuds.
        preset->setCurrentIndex(preset->findData(static_cast<int>(TSA::Model::SupportType::Roller)));
        TEST_CHECK(m.getNode(n1)->support().isRoller() && m.getNode(n2)->support().isRoller(),
                   "Test 222: changement suivant appliqué aux deux nœuds");
        // Les réglages successifs du même nœud sont regroupés en une entrée Annuler : retour à l'état
        // initial (sans appui) pour les deux nœuds à la fois.
        TEST_CHECK(m.canUndo() && m.undo(), "Test 222: entrée Annuler");
        TEST_CHECK(m.getNode(n1)->support().isFree() && m.getNode(n2)->support().isFree(),
                   "Test 222: Annuler rétablit les deux nœuds");
        m.removeObserver(&view);
        std::cout << "[PASS] Test 222: Appui appliqué à tous les nœuds sélectionnés" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 223 : arbre du modèle — liste « Appuis » tenue à jour (BUG-071)
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0);
        const int b = m.addNode(6, 0, 0);
        TSA::UI::ModelTreeWidget tree(&m);
        auto supportsCount = [&tree]() -> int {
            for (auto* t : tree.findChildren<QTreeWidget*>())
            {
                const auto items = t->findItems(QObject::tr("Appuis"), Qt::MatchExactly | Qt::MatchRecursive, 0);
                if (!items.isEmpty()) return items.first()->childCount();
            }
            return -1;
        };
        QCoreApplication::processEvents();
        TEST_CHECK(supportsCount() == 0, "Test 223: aucun appui au départ");

        m.getNode(a)->setSupport(TSA::Model::SupportDefinition::fixed());
        m.notifyNodeModified(a);
        m.getNode(b)->setSupport(TSA::Model::SupportDefinition::pinned());
        m.notifyNodeModified(b);
        QCoreApplication::processEvents();
        TEST_CHECK(supportsCount() == 2, "Test 223: appuis posés → listés (" << supportsCount() << ")");

        m.getNode(a)->setSupport(TSA::Model::SupportDefinition::free());
        m.notifyNodeModified(a);
        QCoreApplication::processEvents();
        TEST_CHECK(supportsCount() == 1, "Test 223: appui retiré → retiré de la liste");

        m.removeNode(b);
        QCoreApplication::processEvents();
        TEST_CHECK(supportsCount() == 0, "Test 223: nœud supprimé → appui retiré de la liste");
        std::cout << "[PASS] Test 223: Liste « Appuis » de l'arbre tenue à jour" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 224 : molette dans le panneau Propriétés — défile, ne change pas les valeurs (BUG-072)
    // -------------------------------------------------------------------------
    {
        Model m;
        const int n = m.addNode(0, 0, 0);
        TSA::UI::PropertyPanel panel(&m);
        panel.resize(320, 400);
        panel.show();
        panel.showNodeProperties(n);
        QCoreApplication::processEvents();
        QComboBox* preset = nullptr;
        for (auto* combo : panel.findChildren<QComboBox*>())
            if (combo->findData(static_cast<int>(TSA::Model::SupportType::Elastic)) >= 0) preset = combo;
        TEST_CHECK(preset != nullptr, "Test 224: liste des types d'appui trouvée");
        const int before = preset->currentIndex();
        preset->clearFocus();
        const QPointF pos(preset->width() / 2.0, preset->height() / 2.0);
        QWheelEvent wheel(pos, preset->mapToGlobal(pos), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(preset, &wheel);
        TEST_CHECK(preset->currentIndex() == before, "Test 224: la molette ne change plus le type d'appui");
        TEST_CHECK(m.getNode(n)->support().isFree(), "Test 224: appui du nœud inchangé");
        panel.hide();
        std::cout << "[PASS] Test 224: Molette du panneau Propriétés sans effet sur les valeurs" << std::endl;
        ++passed;
    }
    return true;
}
