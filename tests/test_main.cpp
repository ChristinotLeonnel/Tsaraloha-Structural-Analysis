#include "test_common.h"
#include <ProductIdentity.h>
#include <cstring>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int passed = 0;
    int expectedTotal = 233;

    std::string suiteFilter = "all";
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--suite=", 8) == 0) {
            suiteFilter = argv[i] + 8;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: " << TSA::Product::kName << "_TestSuite [--suite=all|coordinates|model|io|commands|grids|viewer|cables|extensions|workplane|window|node|loads|opensees|supports|standards|ndc|extraction|ai|preview|thumbnail|engines|tools|mdd|cleanup|bim|snap|automation|blueprint]" << std::endl;
            return 0;
        }
    }

    std::cout << "=================================================" << std::endl;
    std::cout << TSA::Product::kName << " Unit Tests: Modular Verification Suite" << std::endl;
    if (suiteFilter != "all") {
        std::cout << "Filtering by suite: " << suiteFilter << std::endl;
    }
    std::cout << "=================================================" << std::endl;

    bool allOk = true;

    if (suiteFilter == "all" || suiteFilter == "coordinates") {
        std::cout << "\n--- [Suite 1/9] Coordinates, Levels & Snapping (Tests 1-10) ---" << std::endl;
        if (!runSuite_Coordinates(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "model") {
        std::cout << "\n--- [Suite 2/9] Model, Elements & Sections (Tests 11-17, 21-24, 28-29) ---" << std::endl;
        if (!runSuite_Model(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "io" || suiteFilter == "file_io") {
        std::cout << "\n--- [Suite 3/9] File I/O & Persistence (Tests 18, 40, 45) ---" << std::endl;
        if (!runSuite_FileIO(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "commands") {
        std::cout << "\n--- [Suite 4/9] Commands, Undo/Redo & Benchmarks (Tests 19-20, 25, 50) ---" << std::endl;
        if (!runSuite_Commands(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "grids" || suiteFilter == "grid") {
        std::cout << "\n--- [Suite 5/9] Grids & Snapping Systems (Tests 30-33, 202, 207) ---" << std::endl;
        if (!runSuite_Grids(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "viewer") {
        std::cout << "\n--- [Suite 6/9] Viewer, Interaction & Materials (Tests 26-27, 34-35, 208) ---" << std::endl;
        if (!runSuite_Viewer(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "cables" || suiteFilter == "cable") {
        std::cout << "\n--- [Suite 7/9] Cable & Tension Systems (Tests 36, 44, 49) ---" << std::endl;
        if (!runSuite_Cables(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "extensions" || suiteFilter == "tsalib") {
        std::cout << "\n--- [Suite 8/9] Diagnostics & TSALib Extensions (Tests 37-39, 41-43, 46-48) ---" << std::endl;
        if (!runSuite_Extensions(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "workplane" || suiteFilter == "wp") {
        std::cout << "\n--- [Suite 9/10] WorkPlane, LCS & Spatial Snapping (Tests 51-52) ---" << std::endl;
        if (!runSuite_WorkPlane(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "window" || suiteFilter == "windowmanager" || suiteFilter == "layout") {
        std::cout << "\n--- [Suite 10/11] Window Manager, Docks & Layout Profiles (Test 54) ---" << std::endl;
        if (!runSuite_WindowManager(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "node" || suiteFilter == "nodes" || suiteFilter == "point_selector") {
        std::cout << "\n--- [Suite 11/12] Centralized Node System & PointSelector (Tests 53-55) ---" << std::endl;
        if (!runSuite_NodeSystem(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "loads" || suiteFilter == "load") {
        std::cout << "\n--- [Suite 12/13] Structural Loads System (Tests 56-65, 209-211) ---" << std::endl;
        if (!runSuite_Loads(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "opensees" || suiteFilter == "solver") {
        std::cout << "\n--- [Suite 13/14] OpenSees Solver, Immutability & Analytical Validation (Tests 65-69) ---" << std::endl;
        if (!runSuite_OpenSees(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "supports" || suiteFilter == "support") {
        std::cout << "\n--- [Suite 14/14] Structural Supports & 3D Visualization (Tests 70-75) ---" << std::endl;
        if (!runSuite_Supports(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "standards" || suiteFilter == "normative") {
        std::cout << "\n--- [Suite 15/16] Normative Requirements, Annexes & Model Validation (Tests 76-84) ---" << std::endl;
        if (!runSuite_Standards(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "ndc" || suiteFilter == "report") {
        std::cout << "\n--- [Suite 16/16] Professional NDC Report, Eurocodes & Extrema (Tests 85-89) ---" << std::endl;
        if (!runSuite_NDCReport(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "extraction" || suiteFilter == "matrices") {
        std::cout << "\n--- [Suite 17/17] OpenSees Extraction: mapping, K, U, F, transformations (Tests 105-111) ---" << std::endl;
        if (!runSuite_Extraction(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "ai") {
        std::cout << "\n--- [Suite 18/18] AI Co-Engineering: matériel, modèles, vérification, contexte, outils (Tests 112-123) ---" << std::endl;
        if (!runSuite_AI(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "preview") {
        std::cout << "\n--- [Suite 19/19] Projets récents et aperçus du dernier état (Tests 124-126) ---" << std::endl;
        if (!runSuite_Preview(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "engines" || suiteFilter == "analysis") {
        std::cout << "\n--- [Suite 21/21] Analyse multi-moteurs : registre, portée, extraction 2D, validation, UI, remappage (Tests 130-138) ---" << std::endl;
        if (!runSuite_Engines(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "tools" || suiteFilter == "modeling") {
        std::cout << "\n--- [Suite 22/22] Outils de modification et de dessin (Tests 140-149) ---" << std::endl;
        if (!runSuite_ModelingTools(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "mdd" || suiteFilter == "custom2d") {
        std::cout << "\n--- [Suite 23/23] Moteur 2D MetDeDeplacement : formules RDM, rotules, treillis, OpenSees (Tests 150-159) ---" << std::endl;
        if (!runSuite_MetDeDeplacement(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "cleanup") {
        std::cout << "\n--- [Suite 24/24] Nettoyage du modèle (Tests 160-165) ---" << std::endl;
        if (!runSuite_ModelCleanup(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "bim") {
        std::cout << "\n--- [Suite 25/25] Couche BIM : identifiants, mapping physique / analytique, étages, .tsa, IFC (Tests 170-180) ---" << std::endl;
        if (!runSuite_Bim(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "snap") {
        std::cout << "\n--- [Suite 26/26] Accrochage 3D (OSNAP) en espace écran (Tests 181-187) ---" << std::endl;
        if (!runSuite_Snap(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "automation") {
        std::cout << "\n--- [Suite 27/27] Registre central des commandes (Tests 192, 200) ---" << std::endl;
        if (!runSuite_Automation(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "blueprint") {
        std::cout << "--- [Suite 28/28] Blueprint : programmation visuelle, débogueur, scripts (Tests 193-195, 197-199) ---" << std::endl;
        if (!runSuite_Blueprint(passed)) allOk = false;
    }
#ifdef _WIN32
    if (suiteFilter == "all" || suiteFilter == "thumbnail") {
        std::cout << "\n--- [Suite 20/20] Miniatures Explorateur : format 1.2 et " << TSA::Product::kThumbnailProviderDll << " (Tests 127-129) ---" << std::endl;
        if (!runSuite_Thumbnail(passed)) allOk = false;
    }
#endif

    std::cout << "\n=================================================" << std::endl;
    if (suiteFilter == "all") {
        std::cout << "RESULTS: " << passed << " / " << expectedTotal << " tests passed successfully!" << std::endl;
    } else {
        std::cout << "RESULTS: " << passed << " test(s) passed in suite '" << suiteFilter << "'!" << std::endl;
    }
    std::cout << "=================================================" << std::endl;

    return allOk ? 0 : 1;
}
