#include "test_common.h"
#include <cstring>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int passed = 0;
    int expectedTotal = 52;

    std::string suiteFilter = "all";
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--suite=", 8) == 0) {
            suiteFilter = argv[i] + 8;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: TSA_TestSuite [--suite=all|coordinates|model|io|commands|grids|viewer|cables|extensions|workplane]" << std::endl;
            return 0;
        }
    }

    std::cout << "=================================================" << std::endl;
    std::cout << "TSA Unit Tests: Modular Verification Suite" << std::endl;
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
        std::cout << "\n--- [Suite 5/9] Grids & Snapping Systems (Tests 30-33) ---" << std::endl;
        if (!runSuite_Grids(passed)) allOk = false;
    }
    if (suiteFilter == "all" || suiteFilter == "viewer") {
        std::cout << "\n--- [Suite 6/9] Viewer, Interaction & Materials (Tests 26-27, 34-35) ---" << std::endl;
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
        std::cout << "\n--- [Suite 10/10] Window Manager, Docks & Layout Profiles (Test 54) ---" << std::endl;
        if (!runSuite_WindowManager(passed)) allOk = false;
    }

    std::cout << "\n=================================================" << std::endl;
    if (suiteFilter == "all") {
        std::cout << "RESULTS: " << passed << " / " << expectedTotal << " tests passed successfully!" << std::endl;
    } else {
        std::cout << "RESULTS: " << passed << " test(s) passed in suite '" << suiteFilter << "'!" << std::endl;
    }
    std::cout << "=================================================" << std::endl;

    return allOk ? 0 : 1;
}
