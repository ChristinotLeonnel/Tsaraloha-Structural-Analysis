// TSA_CoreTests : programme lié au seul TSA_Core (QtCore, TKernel, TKMath). Preuve à l'édition de liens que le noyau
// ne requiert ni QtGui, ni QtWidgets, ni le rendu OCCT (AIS, V3d), ni un moteur de calcul (TSA Core, étape 1). Couvre
// aussi la frontière avec la couche des bibliothèques : sans fournisseur, catalogues intégrés ; avec, catalogues installés.

#include "Grid/GridManager.h"
#include "Model/DefinitionReference.h"
#include "Model/InstalledCatalogs.h"
#include "Model/Model.h"
#include "UndoRedo/EditTransaction.h"

#include <iostream>

#define CORE_CHECK(cond, msg)                                                                   \
    do                                                                                          \
    {                                                                                           \
        if (!(cond))                                                                            \
        {                                                                                       \
            std::cerr << "[FAIL] " << msg << " (" << #cond << ") at line " << __LINE__ << std::endl; \
            return 1;                                                                           \
        }                                                                                       \
    } while (0)

int main()
{
    using namespace TSA::Model;
    int passed = 0;

    // Modèle : nœuds et barre, transaction (une entrée Annuler), Annuler / Rétablir
    {
        Model m;
        const int a = m.addNode(0.0, 0.0, 0.0), b = m.addNode(6.0, 0.0, 0.0);
        const int bar = m.addBar(a, b, Section::ipe(300), Material::steelS235());
        CORE_CHECK(m.nodes().size() == 2 && bar > 0, "modèle construit (2 nœuds, 1 barre)");
        {
            TSA::UndoRedo::EditTransaction tx(m, "Nœud intermédiaire");
            m.addNode(3.0, 0.0, 0.0);
            tx.commit();
        }
        CORE_CHECK(m.nodes().size() == 3 && m.canUndo(), "transaction validée, une entrée Annuler");
        CORE_CHECK(m.undo() && m.nodes().size() == 2 && m.redo() && m.nodes().size() == 3, "Annuler puis Rétablir");
        const auto snapshot = m.createSnapshot("contrôle");
        m.addNode(9.0, 0.0, 0.0);
        m.restoreSnapshot(snapshot);
        CORE_CHECK(m.nodes().size() == 3, "instantané restauré");
        std::cout << "[PASS] Core: modèle, transaction, historique et instantané" << std::endl;
        ++passed;
    }

    // Catalogues : sans la couche des bibliothèques, catalogue intégré ; un fournisseur installé est lu (et trié)
    {
        CORE_CHECK(!installedCatalogs().sections && !installedCatalogs().cables, "aucun fournisseur dans un programme lié au seul noyau");
        CORE_CHECK(!Section::defaultLibrary().empty() && !CableDefinition::defaultDefinitions().empty(), "catalogues intégrés");
        InstalledCatalogs catalogs;
        catalogs.sections = [] {
            Section z = Section::ipe(200);
            z.name = "Z installé";
            Section a = Section::ipe(100);
            a.name = "A installé";
            return std::vector<Section> { z, a };
        };
        setInstalledCatalogs(catalogs);
        const auto installed = Section::defaultLibrary();
        CORE_CHECK(installed.size() == 2 && installed[0].name == "A installé" && installed[1].name == "Z installé",
                   "catalogue installé lu par l'interface du noyau, trié par nom");
        setInstalledCatalogs({});
        std::cout << "[PASS] Core: catalogues intégrés et installés (interface InstalledCatalogs)" << std::endl;
        ++passed;
    }

    // Grilles (données, QObject et moc du noyau) : grille principale créée par défaut
    {
        TSA::Grid::GridManager grids;
        CORE_CHECK(grids.grids().size() == 1, "gestionnaire de grilles du noyau, grille principale par défaut");
        std::cout << "[PASS] Core: gestionnaire de grilles" << std::endl;
        ++passed;
    }

    // Types du domaine des bibliothèques, définis dans le noyau : version sémantique, référence, propriétés figées
    {
        const auto v = SemanticVersion::fromString("1.2.3-rc1");
        CORE_CHECK(v && v->major == 1 && v->minor == 2 && v->patch == 3 && v->prerelease == "rc1" && v->toString() == "1.2.3-rc1",
                   "version sémantique lue et écrite");
        CORE_CHECK(*v < SemanticVersion(1, 2, 3) && SemanticVersion(1, 2, 3) < SemanticVersion(1, 10, 0) && !SemanticVersion::fromString("abc"),
                   "ordre des versions (pré-version avant la finale, 2 < 10), refus d'une version illisible");
        DefinitionReference ref;
        ref.libraryId = "org.tsaraloha.tsalib";
        ref.definitionId = "steel.ipe200";
        CORE_CHECK(ref.isValid() && ref.toQualifiedKey() == "org.tsaraloha.tsalib:steel.ipe200" && !DefinitionReference {}.isValid(),
                   "référence versionnée vers une définition");
        MechanicalSnapshot a;
        a.youngModulus = 210e9;
        MechanicalSnapshot b = a;
        b.youngModulus = 210e9 * (1.0 + 1e-9);
        MechanicalSnapshot c2 = a;
        c2.youngModulus = 205e9;
        CORE_CHECK(a == b && a != c2, "propriétés mécaniques figées : égalité à la tolérance relative 1e-6");
        std::cout << "[PASS] Core: types du domaine des bibliothèques (SemanticVersion, DefinitionReference, MechanicalSnapshot)" << std::endl;
        ++passed;
    }

    // Historique : comparaison exacte des états fournie par la couche IO (BUG-127). Sans encodeur (programme lié au
    // seul noyau), une transaction sans effet garde l'ancien comportement prudent (une entrée) ; avec, aucune entrée.
    {
        using TSA::UndoRedo::UndoManager;
        Model m;
        m.addNode(0.0, 0.0, 0.0);
        m.clearUndoRedo();
        const auto s = m.createSnapshot("contrôle");
        CORE_CHECK(!UndoManager::sameContent(s, s), "sans encodeur : états jamais déclarés identiques (prudence)");
        {
            TSA::UndoRedo::EditTransaction tx(m, "sans effet");
            tx.commit();
        }
        CORE_CHECK(m.canUndo(), "sans encodeur : transaction sans effet enregistrée (comportement prudent)");
        UndoManager::setContentEncoder([](const Model::ModelStateSnapshot& snap) {
            std::vector<std::uint8_t> bytes;
            for (const auto& [id, n] : snap.nodes)
                for (double v : { double(id), n.x(), n.y(), n.z() })
                {
                    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
                    bytes.insert(bytes.end(), p, p + sizeof v);
                }
            return bytes;
        });
        m.clearUndoRedo();
        {
            TSA::UndoRedo::EditTransaction tx(m, "sans effet");
            tx.commit();
        }
        CORE_CHECK(!m.canUndo(), "avec encodeur : transaction sans effet, aucune entrée");
        {
            TSA::UndoRedo::EditTransaction tx(m, "déplacement");
            m.getNode(m.nodes().begin()->first)->setX(1.0);
            tx.commit();
        }
        CORE_CHECK(m.canUndo(), "avec encodeur : transaction effective, une entrée");
        UndoManager::setContentEncoder({});
        std::cout << "[PASS] Core: comparaison exacte des états par l'historique (encodeur fourni par IO)" << std::endl;
        ++passed;
    }

    std::cout << "RESULTS: " << passed << " / 5 tests passed successfully!" << std::endl;
    return 0;
}
