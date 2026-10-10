// Suite « groupedit » : édition groupée depuis le panneau Propriétés (test 222).
// Plusieurs nœuds sélectionnés, un appui choisi dans les propriétés : tous les nœuds le reçoivent,
// même quand la vue recrée la forme du nœud modifié (ce qui le resélectionnait et interrompait
// l'édition groupée : seul le nœud affiché recevait l'appui, BUG-070).

#include "test_common.h"

#include "UI/Properties/PropertyPanel.h"
#include "Viewer/SelectionManager.h"

#include <QComboBox>

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
    return true;
}
