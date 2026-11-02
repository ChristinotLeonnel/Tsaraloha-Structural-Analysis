#include "test_common.h"
#include "App/ProductInfo.h"
#include "IO/Tsa3d/Tsa3d.h"
#include "IO/TSAEncryption.h"
#include "Analysis/Engine/AnalysisContext.h"
#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Grid/GridManager.h"
#include "Analysis/ResultsModel.h"
#include "Project/ProjectDiagnostics.h"
#include "Project/ProjectManifest.h"
#include "Project/ProjectManager.h"
#include "Project/RecoveryStore.h"
#include <QDir>
#include <QFile>
#include <QSysInfo>
#include <QTemporaryDir>
#include "Standards/ModelValidator.h"
#include <QJsonDocument>
#include <algorithm>
#include <cstring>
#include <functional>

bool runSuite_FileIO(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 18: Native TSA Binary File Format (.tsa) - Persistence & OCCT Rebuild
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 18: TSA File Format Validation Suite ---" << std::endl;

        auto hasCylindricalSurface = [](const TopoDS_Shape& shape) -> bool {
            if (shape.IsNull()) return false;
            TopExp_Explorer exp(shape, TopAbs_FACE);
            for (; exp.More(); exp.Next())
            {
                TopoDS_Face face = TopoDS::Face(exp.Current());
                Handle(Geom_Surface) surf = BRep_Tool::Surface(face);
                if (!surf.IsNull())
                {
                    if (!Handle(Geom_CylindricalSurface)::DownCast(surf).IsNull())
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        const std::string tmpDir = "./build/test_tsa_data/";
        std::filesystem::create_directories(tmpDir);

        // --- SUBTEST 1: Empty project save / load ---
        {
            std::string emptyFile = tmpDir + "test_empty.tsa";
            Model mEmpty;
            std::string err;
            bool saved = TSAProjectIO::saveToFile(QString::fromStdString(emptyFile), mEmpty, nullptr, &err);
            TEST_CHECK(saved, "Subtest 1: Empty project saved successfully");

            Model mEmptyLoaded;
            bool loaded = TSAProjectIO::loadFromFile(QString::fromStdString(emptyFile), mEmptyLoaded, nullptr, &err);
            TEST_CHECK(loaded, "Subtest 1: Empty project loaded successfully");
            TEST_CHECK(mEmptyLoaded.nodes().empty(), "Subtest 1: Loaded empty project has 0 nodes");
            TEST_CHECK(mEmptyLoaded.beams().empty(), "Subtest 1: Loaded empty project has 0 beams");
            std::cout << "  [PASS] Subtest 1: Empty project round-trip" << std::endl;
        }

        // --- SUBTEST 2: Nodes & Bars (2 nodes, 1 bar) ---
        {
            std::string nodeBarFile = tmpDir + "test_node_bar.tsa";
            Model mOrig;
            int n1 = mOrig.addNode(0.0, 0.0, 0.0);
            int n2 = mOrig.addNode(5.0, 0.0, 0.0);
            int b1 = mOrig.addBeam(n1, n2, 0.30, 0.50);

            std::string err;
            bool saved = TSAProjectIO::saveToFile(QString::fromStdString(nodeBarFile), mOrig, nullptr, &err);
            TEST_CHECK(saved, "Subtest 2: Project with nodes & bar saved");

            Model mReloaded;
            bool loaded = TSAProjectIO::loadFromFile(QString::fromStdString(nodeBarFile), mReloaded, nullptr, &err);
            TEST_CHECK(loaded, "Subtest 2: Project with nodes & bar loaded");
            TEST_CHECK(mReloaded.nodes().size() == 2, "Subtest 2: 2 nodes retrieved");
            TEST_CHECK(mReloaded.beams().size() == 1, "Subtest 2: 1 beam retrieved");

            const auto* loadedN1 = mReloaded.getNode(n1);
            const auto* loadedN2 = mReloaded.getNode(n2);
            TEST_CHECK(loadedN1 != nullptr && loadedN2 != nullptr, "Subtest 2: Nodes retrieved by exact ID");
            TEST_CHECK(approxEqual(loadedN1->x(), 0.0) && approxEqual(loadedN1->y(), 0.0) && approxEqual(loadedN1->z(), 0.0), "Subtest 2: Node 1 coords match");
            TEST_CHECK(approxEqual(loadedN2->x(), 5.0) && approxEqual(loadedN2->y(), 0.0) && approxEqual(loadedN2->z(), 0.0), "Subtest 2: Node 2 coords match");

            const auto* loadedB1 = mReloaded.getBeam(b1);
            TEST_CHECK(loadedB1 != nullptr, "Subtest 2: Beam retrieved by exact ID");
            TEST_CHECK(loadedB1->startNodeId() == n1 && loadedB1->endNodeId() == n2, "Subtest 2: Beam node connectivity preserved");
            std::cout << "  [PASS] Subtest 2: Nodes & Bars connectivity round-trip" << std::endl;
        }

        // --- SUBTEST 3: Section Rectangle 400x500 ---
        {
            std::string rectFile = tmpDir + "test_rect.tsa";
            Model mRect;
            int n1 = mRect.addNode(0.0, 0.0, 0.0);
            int n2 = mRect.addNode(6.0, 0.0, 0.0);
            BarProperties bp;
            bp.id = 1;
            bp.name = "Poutre_Rect_400x500";
            bp.role = BarRole::Beam;
            bp.section = Section::rectangular(0.40, 0.50);
            mRect.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(rectFile), mRect, nullptr, &err), "Subtest 3: Save Rectangle");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(rectFile), mLoaded, nullptr, &err), "Subtest 3: Load Rectangle");
            const auto* b = mLoaded.getBar(1);
            TEST_CHECK(b != nullptr, "Subtest 3: Bar found");
            TEST_CHECK(b->section().shape == SectionShape::Rectangular, "Subtest 3: Section shape is Rectangular");
            TEST_CHECK(approxEqual(b->section().width, 0.40), "Subtest 3: Width is 0.40m (400 mm)");
            TEST_CHECK(approxEqual(b->section().height, 0.50), "Subtest 3: Height is 0.50m (500 mm)");
            std::cout << "  [PASS] Subtest 3: Rectangle 400x500 section preserved" << std::endl;
        }

        // --- SUBTEST 4: Section Circle Ø400 ---
        {
            std::string circFile = tmpDir + "test_circ.tsa";
            Model mCirc;
            int n1 = mCirc.addNode(0.0, 0.0, 0.0);
            int n2 = mCirc.addNode(0.0, 0.0, 4.0);
            BarProperties bp;
            bp.id = 2;
            bp.name = "Poteau_Circ_D400";
            bp.role = BarRole::Column;
            bp.section = Section::circular(0.40);
            mCirc.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(circFile), mCirc, nullptr, &err), "Subtest 4: Save Circle");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(circFile), mLoaded, nullptr, &err), "Subtest 4: Load Circle");
            const auto* b = mLoaded.getBar(2);
            TEST_CHECK(b != nullptr, "Subtest 4: Bar found");
            TEST_CHECK(b->section().shape == SectionShape::Circular, "Subtest 4: Section shape is Circular");
            TEST_CHECK(approxEqual(b->section().diameter, 0.40), "Subtest 4: Diameter is 0.40m (400 mm)");
            std::cout << "  [PASS] Subtest 4: Circular D400 section preserved" << std::endl;
        }

        // --- SUBTEST 5: Section IPE200 ---
        {
            std::string ipeFile = tmpDir + "test_ipe.tsa";
            Model mIpe;
            int n1 = mIpe.addNode(0.0, 0.0, 0.0);
            int n2 = mIpe.addNode(4.0, 0.0, 0.0);
            BarProperties bp;
            bp.id = 3;
            bp.name = "Solive_IPE200";
            bp.role = BarRole::Beam;
            bp.section = Section::ipe(200);
            mIpe.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(ipeFile), mIpe, nullptr, &err), "Subtest 5: Save IPE200");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(ipeFile), mLoaded, nullptr, &err), "Subtest 5: Load IPE200");
            const auto* b = mLoaded.getBar(3);
            TEST_CHECK(b != nullptr, "Subtest 5: Bar found");
            TEST_CHECK(b->section().shape == SectionShape::IShape, "Subtest 5: Section shape is IShape");
            TEST_CHECK(b->section().name == "IPE 200", "Subtest 5: Profile name is IPE 200");
            TEST_CHECK(approxEqual(b->section().height, 0.200), "Subtest 5: Height is 0.200m");
            TEST_CHECK(approxEqual(b->section().width, 0.100), "Subtest 5: Width is 0.100m");
            TEST_CHECK(b->section().iy() > 0.0 && b->section().iz() > 0.0, "Subtest 5: Inertias computed properly");
            std::cout << "  [PASS] Subtest 5: IPE 200 profile preserved" << std::endl;
        }

        // --- SUBTEST 6: 3D Diagonal Bar (A, B, direction, length, rotation, releases) ---
        {
            std::string diagFile = tmpDir + "test_diag.tsa";
            Model mDiag;
            int n1 = mDiag.addNode(1.0, 2.0, 3.0);
            int n2 = mDiag.addNode(5.0, 5.0, 7.0);
            BarProperties bp;
            bp.id = 4;
            bp.name = "Bracing_3D";
            bp.role = BarRole::Truss;
            bp.section = Section::boxHollow(0.12, 0.12, 0.008);
            bp.rotation = 37.5;
            bp.endRelease.my = true;
            bp.endRelease.mz = true;
            mDiag.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(diagFile), mDiag, nullptr, &err), "Subtest 6: Save 3D Diagonal");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(diagFile), mLoaded, nullptr, &err), "Subtest 6: Load 3D Diagonal");
            const auto* b = mLoaded.getBar(4);
            TEST_CHECK(b != nullptr, "Subtest 6: Bar found");
            TEST_CHECK(approxEqual(b->length(mLoaded), std::sqrt(16.0 + 9.0 + 16.0)), "Subtest 6: 3D Length is sqrt(41) m");
            TEST_CHECK(approxEqual(b->rotation(), 37.5), "Subtest 6: Rotation is 37.5°");
            TEST_CHECK(b->endRelease().my && b->endRelease().mz, "Subtest 6: End release is Pinned for bending");
            std::cout << "  [PASS] Subtest 6: 3D Diagonal bar orientation & releases preserved" << std::endl;
        }

        // --- SUBTEST 7: Eccentricity (ey, ez) ---
        {
            std::string eccFile = tmpDir + "test_ecc.tsa";
            Model mEcc;
            int n1 = mEcc.addNode(0.0, 0.0, 0.0);
            int n2 = mEcc.addNode(4.0, 0.0, 0.0);
            BarProperties bp;
            bp.id = 5;
            bp.name = "Beam_TopFlange";
            bp.role = BarRole::Beam;
            bp.section = Section::rectangular(0.30, 0.60);
            bp.eccentricity = BarEccentricity::TopFlange;
            mEcc.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(eccFile), mEcc, nullptr, &err), "Subtest 7: Save Eccentricity");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(eccFile), mLoaded, nullptr, &err), "Subtest 7: Load Eccentricity");
            const auto* b = mLoaded.getBar(5);
            TEST_CHECK(b != nullptr, "Subtest 7: Bar found");
            TEST_CHECK(b->eccentricity() == BarEccentricity::TopFlange, "Subtest 7: Eccentricity is TopFlange");
            std::cout << "  [PASS] Subtest 7: Eccentricity preserved" << std::endl;
        }

        // --- SUBTEST 8: Material properties ---
        {
            std::string matFile = tmpDir + "test_mat.tsa";
            Model mMat;
            int n1 = mMat.addNode(0.0, 0.0, 0.0);
            int n2 = mMat.addNode(3.0, 0.0, 0.0);
            Material customMat;
            customMat.name = "AcierHauteLimite_S460";
            customMat.type = MaterialType::Steel;
            customMat.E = 2.10e11;
            customMat.nu = 0.30;
            customMat.density = 7850.0;
            customMat.thermalCoeff = 1.2e-5;
            customMat.fk = 460e6;

            BarProperties bp;
            bp.id = 6;
            bp.section = Section::ipe(300);
            bp.material = customMat;
            mMat.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(matFile), mMat, nullptr, &err), "Subtest 8: Save Material");
            Model mLoaded;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(matFile), mLoaded, nullptr, &err), "Subtest 8: Load Material");
            const auto* b = mLoaded.getBar(6);
            TEST_CHECK(b != nullptr, "Subtest 8: Bar found");
            TEST_CHECK(b->material().name == "AcierHauteLimite_S460", "Subtest 8: Material name preserved");
            TEST_CHECK(approxEqual(b->material().E, 2.10e11), "Subtest 8: Young modulus preserved");
            TEST_CHECK(approxEqual(b->material().nu, 0.30), "Subtest 8: Poisson ratio preserved");
            TEST_CHECK(approxEqual(b->material().density, 7850.0), "Subtest 8: Density preserved");
            TEST_CHECK(approxEqual(b->material().fk, 460e6), "Subtest 8: Yield strength preserved");
            std::cout << "  [PASS] Subtest 8: Material mechanical properties preserved" << std::endl;
        }

        // --- SUBTEST 9: Robustness on corrupted / invalid files ---
        {
            std::string err;
            Model dummyModel;

            // 1. Fichier avec mauvais magic
            std::string badMagicFile = tmpDir + "corrupt_bad_magic.tsa";
            {
                std::ofstream f(badMagicFile, std::ios::binary);
                uint32_t fakeMagic = 0xDEADBEEF;
                f.write(reinterpret_cast<const char*>(&fakeMagic), 4);
                std::vector<char> pad(300, 0);
                f.write(pad.data(), pad.size());
            }
            bool resMagic = TSAProjectIO::loadFromFile(QString::fromStdString(badMagicFile), dummyModel, nullptr, &err);
            TEST_CHECK(!resMagic, "Subtest 9: Bad magic number rejected safely");

            // 2. Fichier tronqué
            std::string truncatedFile = tmpDir + "corrupt_truncated.tsa";
            {
                std::ofstream f(truncatedFile, std::ios::binary);
                uint32_t magic = TSA_FILE_MAGIC;
                f.write(reinterpret_cast<const char*>(&magic), 4);
            }
            bool resTrunc = TSAProjectIO::loadFromFile(QString::fromStdString(truncatedFile), dummyModel, nullptr, &err);
            TEST_CHECK(!resTrunc, "Subtest 9: Truncated file rejected safely");

            // 3. Fichier avec CRC32 corrompu
            std::string badCrcFile = tmpDir + "corrupt_bad_crc.tsa";
            {
                Model mValid;
                mValid.addNode(0, 0, 0);
                TSAProjectIO::saveToFile(QString::fromStdString(badCrcFile), mValid, nullptr);
                std::fstream f(badCrcFile, std::ios::in | std::ios::out | std::ios::binary);
                f.seekp(sizeof(TSAFileHeader) + 5);
                char badByte = static_cast<char>(0xFF);
                f.write(&badByte, 1);
            }
            bool resCrc = TSAProjectIO::loadFromFile(QString::fromStdString(badCrcFile), dummyModel, nullptr, &err);
            TEST_CHECK(!resCrc, "Subtest 9: Corrupted CRC32 data rejected safely");
            std::cout << "  [PASS] Subtest 9: Corrupted files rejected without crashing" << std::endl;
        }

        // --- SUBTEST 10: CRITICAL TEST - Geometric Fidelity & OCCT Reconstruction ---
        {
            std::string criticalFile = tmpDir + "test_critical.tsa";

            // Étape 1 : Création Rectangle 400x500
            Model modelStep1;
            int n1 = modelStep1.addNode(0.0, 0.0, 0.0);
            int n2 = modelStep1.addNode(5.0, 0.0, 0.0);
            BarProperties bp;
            bp.id = 1;
            bp.name = "Poutre_Critique";
            bp.role = BarRole::Beam;
            bp.section = Section::rectangular(0.40, 0.50);
            modelStep1.addBar(bp, n1, n2);

            std::string err;
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(criticalFile), modelStep1, nullptr, &err), "Subtest 10: Step 1 save");

            // Étape 2 : Fermer (clear) et Réouvrir
            Model modelStep2;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(criticalFile), modelStep2, nullptr, &err), "Subtest 10: Step 2 reopen");
            const auto* barLoaded1 = modelStep2.getBar(1);
            TEST_CHECK(barLoaded1 != nullptr, "Subtest 10: Bar retrieved");
            TEST_CHECK(barLoaded1->section().shape == SectionShape::Rectangular, "Subtest 10: Section is Rectangular");

            // Reconstruction OCCT
            TopoDS_Shape shapeRect = BeamGeometry::createBeamShape(*modelStep2.getNode(n1), *modelStep2.getNode(n2), barLoaded1->section());
            TEST_CHECK(!shapeRect.IsNull(), "Subtest 10: OCCT shape built from reloaded model");
            TEST_CHECK(!hasCylindricalSurface(shapeRect), "Subtest 10: OCCT shape is rectangular (not cylindrical)");

            // Étape 3 : Modification Rectangle 400x500 -> Circle Ø400
            BarProperties modifiedBp = barLoaded1->properties();
            modifiedBp.section = Section::circular(0.40);
            modelStep2.getBar(1)->setProperties(modifiedBp);

            // Sauvegarder
            TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(criticalFile), modelStep2, nullptr, &err), "Subtest 10: Step 3 save modified circle");

            // Étape 4 : Fermer et Réouvrir
            Model modelStep3;
            TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(criticalFile), modelStep3, nullptr, &err), "Subtest 10: Step 4 reopen modified project");
            const auto* barFinal = modelStep3.getBar(1);
            TEST_CHECK(barFinal != nullptr, "Subtest 10: Bar retrieved after reopen");
            TEST_CHECK(barFinal->section().shape == SectionShape::Circular, "Subtest 10: Section is Circular");
            TEST_CHECK(approxEqual(barFinal->section().diameter, 0.40), "Subtest 10: Diameter is exactly 0.40m");

            // Reconstruction OCCT finale -> Vérification cylindre réel
            TopoDS_Shape shapeFinal = BeamGeometry::createBeamShape(*modelStep3.getNode(n1), *modelStep3.getNode(n2), barFinal->section());
            TEST_CHECK(!shapeFinal.IsNull(), "Subtest 10: Final OCCT shape created");
            TEST_CHECK(hasCylindricalSurface(shapeFinal), "Subtest 10: Final OCCT shape is a REAL CYLINDER");

            std::cout << "  [PASS] Subtest 10: Critical Test - Exact OCCT shape reconstructed after modify & reload" << std::endl;
        }

        // --- SUBTEST 11: 3D Model Thumbnail Generation & Extraction ---
        {
            std::string thumbFile = tmpDir + "Structure_Reserve.tsa";

            // Modèle avec portique : 4 poteaux, 4 poutres, dalle
            Model modelReserve;
            int n1 = modelReserve.addNode(0, 0, 0);
            int n2 = modelReserve.addNode(6, 0, 0);
            int n3 = modelReserve.addNode(6, 4, 0);
            int n4 = modelReserve.addNode(0, 4, 0);

            int n5 = modelReserve.addNode(0, 0, 3.5);
            int n6 = modelReserve.addNode(6, 0, 3.5);
            int n7 = modelReserve.addNode(6, 4, 3.5);
            int n8 = modelReserve.addNode(0, 4, 3.5);

            // Poteaux
            modelReserve.addColumn(n1, n5, 0.35, 0.35);
            modelReserve.addColumn(n2, n6, 0.35, 0.35);
            modelReserve.addColumn(n3, n7, 0.35, 0.35);
            modelReserve.addColumn(n4, n8, 0.35, 0.35);

            // Poutres
            modelReserve.addBeam(n5, n6, 0.30, 0.50);
            modelReserve.addBeam(n6, n7, 0.30, 0.50);
            modelReserve.addBeam(n7, n8, 0.30, 0.50);
            modelReserve.addBeam(n8, n5, 0.30, 0.50);

            // Dalle
            modelReserve.addSlab({n5, n6, n7, n8}, 0.20);

            // 1. Génération directe de la miniature avec TSAPreviewGenerator
            QImage generatedThumb = TSAPreviewGenerator::generateThumbnail(modelReserve, 512, 512);
            TEST_CHECK(!generatedThumb.isNull(), "Subtest 11: TSAPreviewGenerator produces valid image");
            TEST_CHECK(generatedThumb.width() == 512 && generatedThumb.height() == 512, "Subtest 11: Thumbnail dimensions 512x512");

            // 2. Sauvegarde du fichier Structure_Reserve.tsa avec thumbnail embarqué
            QString saveErr;
            bool saved = TSAProjectIO::saveProject(QString::fromStdString(thumbFile), modelReserve, nullptr,
                                                  "Structure_Reserve", "TSA Architect", true, generatedThumb, &saveErr);
            TEST_CHECK(saved, "Subtest 11: Saved Structure_Reserve.tsa with embedded thumbnail");

            // 3. Extraction rapide de la miniature sans charger le modèle complet
            QImage extractedThumb;
            QString extErr;
            bool extracted = TSAProjectIO::extractThumbnail(QString::fromStdString(thumbFile), extractedThumb, &extErr);
            TEST_CHECK(extracted, "Subtest 11: Fast extractThumbnail succeeded");
            TEST_CHECK(!extractedThumb.isNull(), "Subtest 11: Extracted thumbnail is valid QImage");
            TEST_CHECK(extractedThumb.width() == 512 && extractedThumb.height() == 512, "Subtest 11: Extracted dimensions match");

            // 4. Chargement complet avec récupération du thumbnail
            Model loadedReserve;
            QString loadedProjName, loadedAuthor, loadErr;
            QImage reloadedThumb;
            bool reloaded = TSAProjectIO::loadProject(QString::fromStdString(thumbFile), loadedReserve, nullptr,
                                                     &loadedProjName, &loadedAuthor, &reloadedThumb, &loadErr);
            TEST_CHECK(reloaded, "Subtest 11: Project reloaded completely");
            TEST_CHECK(loadedProjName == "Structure_Reserve", "Subtest 11: Project name preserved");
            TEST_CHECK(loadedAuthor == "TSA Architect", "Subtest 11: Author preserved");
            TEST_CHECK(!reloadedThumb.isNull(), "Subtest 11: Thumbnail loaded alongside project");
            TEST_CHECK(loadedReserve.nodes().size() == 8, "Subtest 11: 8 nodes preserved");
            TEST_CHECK(loadedReserve.columns().size() == 4, "Subtest 11: 4 columns preserved");
            TEST_CHECK(loadedReserve.beams().size() == 4, "Subtest 11: 4 beams preserved");
            TEST_CHECK(loadedReserve.slabs().size() == 1, "Subtest 11: 1 slab preserved");

            std::cout << "  [PASS] Subtest 11: 3D Thumbnail generated, embedded in .tsa, and extracted successfully" << std::endl;
        }

        std::cout << "[PASS] Test 18: Full TSA Native File Format Suite (11 Subtests Validated)" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------
    // TEST 40: TSALib Phase 3 - Manifest Format & Disk Layout
    // -------------------------------------------------------------
    {
        std::cout << "\n--- TEST 40: TSALib Phase 3 - Manifest Format & Disk Layout ---" << std::endl;

        // 40.1: TSALib manifest.json Existence & Semantics
        {
            QString manifestPath = TSA::Product::sourceDirectory() + "/Extensions/TSALib/manifest.json";
            TEST_CHECK(QFile::exists(manifestPath), "Subtest 40.1: Extensions/TSALib/manifest.json exists on disk");

            QFile f(manifestPath);
            TEST_CHECK(f.open(QIODevice::ReadOnly), "Subtest 40.1: manifest.json is readable");

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            TEST_CHECK(err.error == QJsonParseError::NoError, "Subtest 40.1: manifest.json is valid JSON");

            std::string parseErr;
            auto manifest = TSA::ExtensionSystem::ExtensionManifest::fromJson(doc.object(), &parseErr);
            TEST_CHECK(manifest.has_value(), "Subtest 40.1: manifest parsed via ExtensionManifest::fromJson");
            TEST_CHECK(manifest->id == "org.tsaraloha.tsalib", "Subtest 40.1: Official ID is org.tsaraloha.tsalib");
            TEST_CHECK(manifest->name == "TSA Engineering Library", "Subtest 40.1: Official Name is TSA Engineering Library");
            TEST_CHECK(manifest->version == TSA::ExtensionSystem::SemanticVersion(1, 0, 0), "Subtest 40.1: Version is 1.0.0");
            TEST_CHECK(manifest->kind == TSA::ExtensionSystem::ExtensionKind::DataExtension, "Subtest 40.1: Kind is DataExtension");
            TEST_CHECK(manifest->categories.size() >= 8, "Subtest 40.1: At least 8 engineering categories declared");

            std::cout << "  [PASS] Subtest 40.1: TSALib manifest.json Existence & Semantics Verified" << std::endl;
        }

        // 40.2: LibraryValidator Full Directory Validation
        {
            TSA::ExtensionSystem::LibraryValidator validator;
            auto valRes = validator.validateExtensionDirectory(TSA::Product::sourceDirectory() + "/Extensions/TSALib");
            TEST_CHECK(valRes.valid, "Subtest 40.2: Extensions/TSALib directory validates without errors");

            std::cout << "  [PASS] Subtest 40.2: LibraryValidator Full Directory Validation Verified" << std::endl;
        }

        // 40.3: Standards Directory & Reference Specifications
        {
            QString standardsDir = TSA::Product::sourceDirectory() + "/Extensions/TSALib/Standards";
            TEST_CHECK(QDir(standardsDir).exists(), "Subtest 40.3: Standards directory exists");

            QString en1990 = standardsDir + "/EN1990.json";
            QString en1992 = standardsDir + "/EN1992.json";
            QString en1993 = standardsDir + "/EN1993.json";
            QString en10138 = standardsDir + "/EN10138.json";

            TEST_CHECK(QFile::exists(en1990), "Subtest 40.3: EN1990.json exists");
            TEST_CHECK(QFile::exists(en1992), "Subtest 40.3: EN1992.json exists");
            TEST_CHECK(QFile::exists(en1993), "Subtest 40.3: EN1993.json exists");
            TEST_CHECK(QFile::exists(en10138), "Subtest 40.3: EN10138.json exists");

            // Vérification de contenu d'une fiche normative
            QFile f1992(en1992);
            TEST_CHECK(f1992.open(QIODevice::ReadOnly), "Subtest 40.3: EN1992.json readable");
            QJsonDocument doc = QJsonDocument::fromJson(f1992.readAll());
            TEST_CHECK(doc.object()["standard"].toString() == "EN 1992-1-1", "Subtest 40.3: EN 1992-1-1 code match");

            std::cout << "  [PASS] Subtest 40.3: Standards Directory & Reference Specifications Verified" << std::endl;
        }

        // 40.4: LibraryManager Auto-Discovery of TSALib on Disk
        {
            auto& libMgr = TSA::ExtensionSystem::LibraryManager::instance();
            libMgr.addSearchPath(TSA::Product::sourceDirectory() + "/Extensions");
            auto discovered = libMgr.discover();

            bool foundTSALib = false;
            for (const auto& ext : discovered)
            {
                if (ext.id == "org.tsaraloha.tsalib")
                {
                    foundTSALib = true;
                    break;
                }
            }
            TEST_CHECK(foundTSALib, "Subtest 40.4: TSALib extension auto-discovered by LibraryManager");

            std::cout << "  [PASS] Subtest 40.4: LibraryManager Auto-Discovery of TSALib on Disk Verified" << std::endl;
        }

        std::cout << "[PASS] Test 40: TSALib Phase 3 - Manifest Format & Disk Layout (4 Subtests Validated) Passed Successfully!" << std::endl;
        passed++;
    }


    // -------------------------------------------------------------------------
    // TEST 45: TSALib Phase 8 - Versioning & Snapshots de Calcul dans le format .tsa
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 45: TSALib Phase 8 - Versioning & Snapshots de Calcul dans le format .tsa ---" << std::endl;

        // 45.1: Generation et Affectation de MechanicalSnapshot & DefinitionReference
        TSA::Model::Model testModel;
        int n1 = testModel.addNode(0.0, 0.0, 0.0, "", "N1");
        int n2 = testModel.addNode(5.0, 0.0, 0.0, "", "N2");
        int n3 = testModel.addNode(5.0, 0.0, 3.0, "", "N3");

        auto matConcrete = TSA::Model::Material::concreteC25_30();
        auto matSteel = TSA::Model::Material::steelS355();
        auto secBeam = TSA::Model::Section::rectangular(0.30, 0.50);
        auto secCol = TSA::Model::Section::rectangular(0.30, 0.30);

        testModel.addBar(n1, n2, secBeam, matConcrete, TSA::Model::BarRole::Beam, 0.0, "Poutre B1");
        testModel.addBar(n2, n3, secCol, matSteel, TSA::Model::BarRole::Column, 0.0, "Poteau C1");

        // Affectation explicite de snapshots de calcul immuables
        TSA::ExtensionSystem::MechanicalSnapshot concreteSnap;
        concreteSnap.youngModulus = 31.0e9;
        concreteSnap.poissonRatio = 0.20;
        concreteSnap.density = 2500.0;
        concreteSnap.characteristicStrength = 25.0e6;
        concreteSnap.yieldStrength = 25.0e6;
        concreteSnap.thermalCoeff = 1.0e-5;

        TSA::ExtensionSystem::DefinitionReference concreteRef;
        concreteRef.libraryId = "org.tsaraloha.tsalib";
        concreteRef.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
        concreteRef.definitionId = "concrete.c25_30";
        concreteRef.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);

        testModel.setCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30", concreteSnap, concreteRef);

        TSA::ExtensionSystem::MechanicalSnapshot steelSnap;
        steelSnap.youngModulus = 210.0e9;
        steelSnap.poissonRatio = 0.30;
        steelSnap.density = 7850.0;
        steelSnap.characteristicStrength = 355.0e6;
        steelSnap.yieldStrength = 355.0e6;
        steelSnap.thermalCoeff = 1.2e-5;

        TSA::ExtensionSystem::DefinitionReference steelRef;
        steelRef.libraryId = "org.tsaraloha.tsalib";
        steelRef.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
        steelRef.definitionId = "steel.s355";
        steelRef.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);

        testModel.setCalculationSnapshot("org.tsaraloha.tsalib:steel.s355", steelSnap, steelRef);

        // Ajout d'un cable avec definition catalogue et snapshot
        auto& cableLib = TSA::Library::CableLibrary::instance();
        cableLib.reloadFromRegistry();
        const auto* foundCableDef = cableLib.findByName("Stay PSS 19x15.7mm");
        if (foundCableDef)
        {
            testModel.addCable(n1, n3, *foundCableDef, "Hauban H1");
            TSA::ExtensionSystem::MechanicalSnapshot cableSnap;
            cableSnap.youngModulus = foundCableDef->elasticModulus();
            cableSnap.poissonRatio = 0.30;
            cableSnap.density = foundCableDef->density();
            cableSnap.characteristicStrength = foundCableDef->characteristicStrength();
            cableSnap.yieldStrength = foundCableDef->minimumBreakingForce() / foundCableDef->metallicArea();
            cableSnap.thermalCoeff = 1.2e-5;

            TSA::ExtensionSystem::DefinitionReference cableRef;
            cableRef.libraryId = "org.tsaraloha.tsalib";
            cableRef.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
            cableRef.definitionId = "stay_pss_19_15_7";
            cableRef.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);

            testModel.setCalculationSnapshot("org.tsaraloha.tsalib:stay_pss_19_15_7", cableSnap, cableRef);
        }

        TEST_CHECK(testModel.hasCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30"), "Subtest 45.1: Snapshot beton enregistre");
        TEST_CHECK(testModel.hasCalculationSnapshot("org.tsaraloha.tsalib:steel.s355"), "Subtest 45.1: Snapshot acier enregistre");
        TEST_CHECK(testModel.calculationSnapshots().size() >= 2, "Subtest 45.1: Au moins 2 snapshots presents");
        std::cout << "  [PASS] Subtest 45.1: Generation et Affectation des Snapshots de Calcul Validees" << std::endl;

        // 45.2: Serialisation binaire dans le format .tsa avec CHUNK_SNAP
        std::string testFilePath = "test_phase8_snapshot.tsa";
        TSA::IO::TSAFileWriter writer;
        writer.setCompressionEnabled(true);
        std::string saveErr;
        bool saved = writer.saveToFile(testFilePath, testModel, nullptr, "Projet Phase 8", "TSA Testing", &saveErr);
        TEST_CHECK(saved, "Subtest 45.2: Sauvegarde fichier .tsa reussie");
        TEST_CHECK(std::filesystem::exists(testFilePath), "Subtest 45.2: Fichier .tsa cree sur disque");

        // Verification de la presence du header valide
        TSA::IO::TSAFileHeader header;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(testFilePath, header), "Subtest 45.2: En-tete binaire lisible");
        TEST_CHECK(header.magic == TSA::IO::NATIVE_FILE_MAGIC, "Subtest 45.2: Magic native du produit valide");
        TEST_CHECK(header.checksumCRC32 != 0, "Subtest 45.2: Checksum CRC32 present");
        std::cout << "  [PASS] Subtest 45.2: Serialisation Binaire avec CHUNK_SNAP Validee" << std::endl;

        // 45.3: Deserialisation et Verification Bitwise des Snapshots et References
        TSA::Model::Model loadedModel;
        TSA::IO::TSAFileReader reader;
        std::string loadErr, outProj, outAuth;
        bool loaded = reader.loadFromFile(testFilePath, loadedModel, nullptr, "", &outProj, &outAuth, nullptr, &loadErr);
        TEST_CHECK(loaded, "Subtest 45.3: Chargement du fichier .tsa reussi");
        TEST_CHECK(outProj == "Projet Phase 8", "Subtest 45.3: Nom de projet conforme");

        TEST_CHECK(loadedModel.hasCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30"), "Subtest 45.3: Snapshot beton recupere");
        const auto* loadedConcreteSnap = loadedModel.getCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30");
        TEST_CHECK(loadedConcreteSnap != nullptr, "Subtest 45.3: Pointeur snapshot non-nul");
        if (loadedConcreteSnap)
        {
            TEST_CHECK(approxEqual(loadedConcreteSnap->youngModulus, 31.0e9), "Subtest 45.3: E = 31 GPa bitwise exact");
            TEST_CHECK(approxEqual(loadedConcreteSnap->poissonRatio, 0.20), "Subtest 45.3: nu = 0.20 exact");
            TEST_CHECK(approxEqual(loadedConcreteSnap->density, 2500.0), "Subtest 45.3: rho = 2500 kg/m3 exact");
            TEST_CHECK(approxEqual(loadedConcreteSnap->characteristicStrength, 25.0e6), "Subtest 45.3: fck = 25 MPa exact");
            TEST_CHECK(approxEqual(loadedConcreteSnap->thermalCoeff, 1.0e-5), "Subtest 45.3: alpha = 1.0e-5 exact");
        }

        const auto* loadedConcreteRef = loadedModel.getDefinitionReference("org.tsaraloha.tsalib:concrete.c25_30");
        TEST_CHECK(loadedConcreteRef != nullptr, "Subtest 45.3: DefinitionReference presente");
        if (loadedConcreteRef)
        {
            TEST_CHECK(loadedConcreteRef->libraryId == "org.tsaraloha.tsalib", "Subtest 45.3: Library ID conforme");
            TEST_CHECK(loadedConcreteRef->libraryVersion.toString() == "1.0.0", "Subtest 45.3: Library Version 1.0.0");
            TEST_CHECK(loadedConcreteRef->definitionId == "concrete.c25_30", "Subtest 45.3: Definition ID conforme");
        }

        // Test de persistance Undo/Redo dans le modele charge
        auto undoSnapshot = loadedModel.createSnapshot("Test Undo");
        TEST_CHECK(undoSnapshot.calculationSnapshots.size() >= 2, "Subtest 45.3: Snapshots inclus dans ModelStateSnapshot");
        std::filesystem::remove(testFilePath);
        std::cout << "  [PASS] Subtest 45.3: Deserialisation et Verification Bitwise des Snapshots Validees" << std::endl;

        // 45.4: Simulation d'Evolution Normative de TSALib & Detection par LibraryVersionManager
        TSA::ExtensionSystem::LibraryVersionManager versionMgr;

        // Cas A : Definition identique -> 0 differences mecaniques
        TSA::ExtensionSystem::MaterialDefinition identicalDef;
        identicalDef.id = "concrete.c25_30";
        identicalDef.version = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
        identicalDef.youngModulus = TSA::ExtensionSystem::PhysicalValue(31000.0, "MPa");
        identicalDef.poissonRatio = 0.20;
        identicalDef.density = TSA::ExtensionSystem::PhysicalValue(2500.0, "kg/m3");
        identicalDef.fck = TSA::ExtensionSystem::PhysicalValue(25.0, "MPa");
        identicalDef.fy = TSA::ExtensionSystem::PhysicalValue(25.0, "MPa");
        identicalDef.thermalCoeff = TSA::ExtensionSystem::PhysicalValue(1.0e-5, "1/K");

        auto resultIdentical = versionMgr.compare(*loadedConcreteSnap, TSA::ExtensionSystem::SemanticVersion(1, 0, 0), identicalDef);
        TEST_CHECK(!resultIdentical.hasMechanicalChanges(), "Subtest 45.4: Aucune divergence detectee sur version identique");

        // Cas B : Mise a jour normative TSALib (v1.1.0 avec E = 35 GPa et fck = 28 MPa)
        TSA::ExtensionSystem::MaterialDefinition modifiedDef = identicalDef;
        modifiedDef.version = TSA::ExtensionSystem::SemanticVersion(1, 1, 0);
        modifiedDef.youngModulus = TSA::ExtensionSystem::PhysicalValue(35000.0, "MPa"); // Modifie
        modifiedDef.fck = TSA::ExtensionSystem::PhysicalValue(28.0, "MPa");            // Modifie
        modifiedDef.fy = TSA::ExtensionSystem::PhysicalValue(28.0, "MPa");             // Modifie

        auto resultModified = versionMgr.compare(*loadedConcreteSnap, TSA::ExtensionSystem::SemanticVersion(1, 0, 0), modifiedDef);
        TEST_CHECK(resultModified.hasMechanicalChanges(), "Subtest 45.4: Divergence mecanique detectee avec succes");
        TEST_CHECK(resultModified.modifiedProperties.size() == 3, "Subtest 45.4: Exactement 3 proprietes physiques modifiees (E, fck et fy)");

        // Verification que le snapshot du projet en memoire est reste INALTERE (garantie de reproductibilite)
        const auto* preservedSnap = loadedModel.getCalculationSnapshot("org.tsaraloha.tsalib:concrete.c25_30");
        TEST_CHECK(approxEqual(preservedSnap->youngModulus, 31.0e9), "Subtest 45.4: E du projet conserve a 31 GPa (immuabilite garantie)");
        TEST_CHECK(approxEqual(preservedSnap->characteristicStrength, 25.0e6), "Subtest 45.4: fck du projet conserve a 25 MPa (immuabilite garantie)");

        std::cout << "  [PASS] Subtest 45.4: Detection de Derive Normative & Garantie de Reproductibilite Validees" << std::endl;

        std::cout << "[PASS] Test 45: TSALib Phase 8 - Versioning & Snapshots de Calcul dans le format .tsa (4 Subtests Valides) Passed Successfully!" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 90: Stress .tsa — portiques 3D volumineux (Save / Load chronométrés)
    // Les fichiers générés (build/test_tsa_data/stress_*.tsa) servent aussi de
    // jeux d'essai pour mesurer l'ouverture dans l'application (log ProjectLoadTiming).
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 90: Stress .tsa (portiques 3D volumineux) ---" << std::endl;
        const std::string tmpDir = "./build/test_tsa_data/";
        std::filesystem::create_directories(tmpDir);

        auto buildFrame = [](Model& m, int nx, int ny, int stories) {
            const double span = 5.0;
            const double storyHeight = 3.0;
            std::vector<int> ids(static_cast<size_t>(nx * ny * (stories + 1)));
            auto idx = [&](int i, int j, int k) { return static_cast<size_t>((k * ny + j) * nx + i); };
            for (int k = 0; k <= stories; ++k)
                for (int j = 0; j < ny; ++j)
                    for (int i = 0; i < nx; ++i)
                        ids[idx(i, j, k)] = m.addNode(i * span, j * span, k * storyHeight);
            for (int k = 0; k < stories; ++k)
                for (int j = 0; j < ny; ++j)
                    for (int i = 0; i < nx; ++i)
                        m.addColumn(ids[idx(i, j, k)], ids[idx(i, j, k + 1)]);
            for (int k = 1; k <= stories; ++k)
                for (int j = 0; j < ny; ++j)
                    for (int i = 0; i < nx; ++i)
                    {
                        if (i + 1 < nx) m.addBeam(ids[idx(i, j, k)], ids[idx(i + 1, j, k)]);
                        if (j + 1 < ny) m.addBeam(ids[idx(i, j, k)], ids[idx(i, j + 1, k)]);
                    }
        };

        struct StressCase { const char* name; int nx; int ny; int stories; };
        const StressCase cases[] = { { "stress_1400", 8, 8, 8 }, { "stress_4900", 12, 12, 12 } };
        for (const auto& c : cases)
        {
            Model m;
            buildFrame(m, c.nx, c.ny, c.stories);
            const size_t bars = m.beams().size() + m.columns().size();
            const std::string file = tmpDir + c.name + ".tsa";

            std::string err;
            auto t0 = std::chrono::steady_clock::now();
            bool saved = TSAProjectIO::saveToFile(QString::fromStdString(file), m, nullptr, &err);
            auto t1 = std::chrono::steady_clock::now();
            TEST_CHECK(saved, "Test 90: sauvegarde du modèle de stress");

            Model reloaded;
            bool loaded = TSAProjectIO::loadFromFile(QString::fromStdString(file), reloaded, nullptr, &err);
            auto t2 = std::chrono::steady_clock::now();
            TEST_CHECK(loaded, "Test 90: chargement du modèle de stress");
            TEST_CHECK(reloaded.nodes().size() == m.nodes().size(), "Test 90: nombre de nœuds conservé");
            TEST_CHECK(reloaded.beams().size() == m.beams().size(), "Test 90: nombre de poutres conservé");
            TEST_CHECK(reloaded.columns().size() == m.columns().size(), "Test 90: nombre de poteaux conservé");

            const auto saveMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            const auto loadMs = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            std::cout << "  [Stress] " << c.name << " : " << m.nodes().size() << " nœuds, " << bars
                      << " barres | Save = " << saveMs << " ms | Load = " << loadMs << " ms | "
                      << std::filesystem::file_size(file) / 1024 << " Kio" << std::endl;
        }

        std::cout << "[PASS] Test 90: Stress .tsa Save/Load" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 91: Persistance des charges (chunk LOAD, format 1.1) & compatibilité 1.0
    // Régression : les cas de charge, combinaisons, charges nodales et charges sur barres
    // n'étaient jamais écrits ; un projet rouvert n'avait plus AUCUN cas de charge.
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 91: Persistance des charges (.tsa 1.1) ---" << std::endl;
        const std::string tmpDir = "./build/test_tsa_data/";
        std::filesystem::create_directories(tmpDir);
        const std::string file = tmpDir + "test_loads_roundtrip.tsa";

        Model m;
        int n1 = m.addNode(0.0, 0.0, 0.0);
        int n2 = m.addNode(6.0, 0.0, 0.0);
        int n3 = m.addNode(6.0, 0.0, 3.0);
        int b1 = m.addBeam(n1, n2);
        int c1 = m.addColumn(n2, n3);

        auto& lm = m.loadManager();
        int customCase = lm.addLoadCase(LoadCase(0, "Q_toiture", LoadCaseCategory::Live, false, 1.0, "Entretien toiture"));
        LoadCombination combo(0, "ELU perso", LoadCombinationType::ULS_Fundamental);
        combo.setFactor(1, 1.35);
        combo.setFactor(customCase, 1.5);
        int comboId = lm.addCombination(combo);
        int nlId = lm.addNodalLoad(NodalLoad(0, n3, customCase, 10.0, -2.5, -30.0, 0.5, 1.5, -4.0, LoadCoordSystem::Global, "P_tete"));
        int mlId = lm.addMemberLoad(MemberLoad::trapezoidal(0, b1, 1, -5.0, -8.0, 0.1, 0.9, LoadDirection::GlobalZ, true, "q_trap"));
        MemberLoad colLoad = MemberLoad::uniform(0, c1, customCase, 2.0, LoadDirection::GlobalX, "vent_poteau", MemberTargetType::Column);
        int colLoadId = lm.addMemberLoad(colLoad);
        lm.setActiveLoadCaseId(customCase);

        std::string err;
        TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(file), m, nullptr, &err), "Test 91: sauvegarde avec charges");

        Model r;
        TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(file), r, nullptr, &err), "Test 91: rechargement avec charges");
        const auto& rl = r.loadManager();
        TEST_CHECK(rl.loadCases().size() == lm.loadCases().size(), "Test 91: nombre de cas de charge conservé");
        TEST_CHECK(rl.combinations().size() == lm.combinations().size(), "Test 91: nombre de combinaisons conservé");
        TEST_CHECK(rl.activeLoadCaseId() == customCase, "Test 91: cas actif conservé");

        const auto* lc = rl.getLoadCase(customCase);
        TEST_CHECK(lc && lc->name() == "Q_toiture" && lc->category() == LoadCaseCategory::Live &&
                   lc->description() == "Entretien toiture", "Test 91: cas personnalisé conservé");
        TEST_CHECK(rl.getLoadCase(1) && rl.getLoadCase(1)->isSelfWeightIncluded(), "Test 91: poids propre du cas G conservé");

        const auto* rc = rl.getCombination(comboId);
        TEST_CHECK(rc && rc->name() == "ELU perso" && approxEqual(rc->factor(1), 1.35) && approxEqual(rc->factor(customCase), 1.5),
                   "Test 91: facteurs de combinaison conservés");

        const auto* rn = rl.getNodalLoad(nlId);
        TEST_CHECK(rn && rn->nodeId() == n3 && rn->loadCaseId() == customCase && rn->name() == "P_tete", "Test 91: charge nodale conservée");
        TEST_CHECK(approxEqual(rn->fx(), 10.0) && approxEqual(rn->fy(), -2.5) && approxEqual(rn->fz(), -30.0) &&
                   approxEqual(rn->mx(), 0.5) && approxEqual(rn->my(), 1.5) && approxEqual(rn->mz(), -4.0),
                   "Test 91: composantes de la charge nodale exactes");

        const auto* rm = rl.getMemberLoad(mlId);
        TEST_CHECK(rm && rm->elementId() == b1 && rm->type() == LoadType::MemberLinear && rm->isRelativePosition() &&
                   approxEqual(rm->q1(), -5.0) && approxEqual(rm->q2(), -8.0) && approxEqual(rm->x1(), 0.1) &&
                   approxEqual(rm->x2(), 0.9) && rm->direction() == LoadDirection::GlobalZ,
                   "Test 91: charge trapézoïdale conservée");
        const auto* rcl = rl.getMemberLoad(colLoadId);
        TEST_CHECK(rcl && rcl->targetType() == MemberTargetType::Column && rcl->elementId() == c1,
                   "Test 91: cible (poteau) de la charge sur barre conservée");

        // Les identifiants suivants ne doivent pas entrer en collision avec les charges rechargées
        Model& rw = r;
        int nextNl = rw.loadManager().addNodalLoad(NodalLoad(0, n1, 1, 0.0, 0.0, -1.0));
        TEST_CHECK(nextNl > nlId, "Test 91: pas de collision d'identifiant après rechargement");

        // --- Compatibilité : fichier 1.0 sans chunk LOAD -> cas Eurocodes par défaut
        const std::string legacyFile = tmpDir + "test_loads_legacy_v10.tsa";
        TEST_CHECK(TSAProjectIO::saveProject(QString::fromStdString(file), m, nullptr, "legacy", "tests", false, QImage(), nullptr),
                   "Test 91: sauvegarde non compressée");
        {
            std::ifstream in(file, std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            TSAFileHeader h;
            std::memcpy(&h, bytes.data(), sizeof(h));
            std::vector<uint8_t> payload;
            size_t off = sizeof(h);
            bool sawLoad = false;
            // Le payload s'arrête à header.fileSize (format 1.2 : bloc d'aperçu ensuite).
            const size_t payloadEnd = std::min<size_t>(bytes.size(), static_cast<size_t>(h.fileSize));
            while (off + sizeof(TSAChunkHeader) <= payloadEnd)
            {
                TSAChunkHeader ch;
                std::memcpy(&ch, bytes.data() + off, sizeof(ch));
                const size_t total = sizeof(ch) + ch.chunkSize;
                if (off + total > payloadEnd) break;
                if (ch.chunkId == CHUNK_LOAD) sawLoad = true;
                else payload.insert(payload.end(), bytes.begin() + off, bytes.begin() + off + total);
                off += total;
            }
            TEST_CHECK(sawLoad, "Test 91: le chunk LOAD est bien écrit");
            h.versionMinor = 0;
            h.flags &= ~FLAG_HAS_PREVIEW_BLOCK; // un fichier 1.0 n'a pas de bloc d'aperçu
            h.checksumCRC32 = computeCRC32(payload.data(), payload.size());
            h.uncompressedSize = payload.size();
            h.fileSize = sizeof(h) + payload.size();
            std::ofstream out(legacyFile, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(&h), sizeof(h));
            out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        }
        Model legacy;
        TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(legacyFile), legacy, nullptr, &err), "Test 91: fichier 1.0 lisible");
        TEST_CHECK(legacy.beams().size() == 1 && legacy.columns().size() == 1, "Test 91: géométrie du fichier 1.0 intacte");
        TEST_CHECK(legacy.loadManager().loadCases().size() == 5, "Test 91: fichier 1.0 -> 5 cas Eurocodes par défaut (G, Q, W, S, E)");
        TEST_CHECK(!legacy.loadManager().combinations().empty(), "Test 91: fichier 1.0 -> combinaisons par défaut");
        TEST_CHECK(legacy.loadManager().nodalLoads().empty() && legacy.loadManager().memberLoads().empty(),
                   "Test 91: fichier 1.0 -> aucune charge fantôme");

        // --- Sauvegarde atomique : réécrire par-dessus un fichier existant reste valide
        TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(file), r, nullptr, &err), "Test 91: réécriture du fichier existant");
        Model again;
        TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(file), again, nullptr, &err) &&
                   again.loadManager().memberLoads().size() == r.loadManager().memberLoads().size(),
                   "Test 91: fichier réécrit valide");
        // Échec d'écriture (dossier inexistant) : erreur explicite, pas d'exception
        TEST_CHECK(!TSAProjectIO::saveToFile(QString::fromStdString(tmpDir + "dossier_inexistant/x.tsa"), r, nullptr, &err) && !err.empty(),
                   "Test 91: échec d'écriture signalé");

        std::cout << "[PASS] Test 91: Persistance des charges & compatibilité .tsa 1.0" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 92: CRC32 par table == CRC32 IEEE 802.3 de référence
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 92: CRC32 (table) ---" << std::endl;
        const std::string sample = "123456789";
        TEST_CHECK(computeCRC32(reinterpret_cast<const uint8_t*>(sample.data()), sample.size()) == 0xCBF43926u,
                   "Test 92: valeur de contrôle CRC-32/ISO-HDLC de '123456789'");
        TEST_CHECK(computeCRC32(nullptr, 0) == 0u, "Test 92: CRC d'un tampon vide");
        std::cout << "[PASS] Test 92: CRC32" << std::endl;
        passed++;
    }


    // TEST 189 : paramètres d'analyse persistés (chunk SETT, format 1.4) — BUG-013
    {
        const std::string tmpDir = "./build/test_tsa_data/";
        std::filesystem::create_directories(tmpDir);
        const std::string file = tmpDir + "test_settings_roundtrip.tsa";

        TSA::Analysis::AnalysisContext ctx;
        ctx.engineId = "custom2d";
        ctx.combinationId = 3;
        ctx.common.includeSelfWeight = false;
        ctx.loadCaseIds = { 1, 2 };
        Model m;
        m.addNode(0, 0, 0);
        m.setAnalysisSettingsJson(QJsonDocument(ctx.toJson()).toJson(QJsonDocument::Compact).toStdString());
        std::string err;
        TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(file), m, nullptr, &err), "Test 189: sauvegarde avec paramètres");

        Model r;
        r.setAnalysisSettingsJson("{\"stale\":true}");   // un projet précédent ne doit pas déteindre
        TEST_CHECK(TSAProjectIO::loadFromFile(QString::fromStdString(file), r, nullptr, &err), "Test 189: rechargement");
        bool ok = false;
        const auto back = TSA::Analysis::AnalysisContext::fromJson(
            QJsonDocument::fromJson(QByteArray::fromStdString(r.analysisSettingsJson())).object(), &ok);
        TEST_CHECK(ok && back.engineId == "custom2d" && back.combinationId == 3 && !back.common.includeSelfWeight
                       && back.loadCaseIds == std::vector<int>({ 1, 2 }),
                   "Test 189: paramètres relus à l'identique");

        Model plain;
        plain.addNode(0, 0, 0);
        const std::string file2 = tmpDir + "test_settings_none.tsa";
        TEST_CHECK(TSAProjectIO::saveToFile(QString::fromStdString(file2), plain, nullptr, &err)
                       && TSAProjectIO::loadFromFile(QString::fromStdString(file2), r, nullptr, &err)
                       && r.analysisSettingsJson().empty(),
                   "Test 189: sans chunk SETT → réglages par défaut");
        std::cout << "[PASS] Test 189: Paramètres d'analyse persistés (.tsa 1.4)" << std::endl;
        ++passed;
    }

    // TEST 201 : audit 2026-10-09 — chemins non ASCII, chaînes ≥ 64 Kio, nom / auteur du projet
    {
        const QString dir = QDir::currentPath() + QString::fromUtf8("/build/test_tsa_data/Études Bâtiment");
        QDir().mkpath(dir);
        const QString file = dir + QString::fromUtf8("/Pont à poutres.tsa");
        QFile::remove(file);

        // 1. Chemin accentué : enregistré par QSaveFile, relu par std::ifstream (page de code ANSI auparavant)
        Model m;
        const int a = m.addNode(0, 0, 0);
        const int b = m.addNode(6, 0, 0);
        m.addBeam(a, b);
        std::string err;
        TEST_CHECK(TSAProjectIO::saveToFile(file, m, nullptr, &err), "Test 201: enregistrement sous un chemin accentué");
        Model r;
        err.clear();
        TEST_CHECK(TSAProjectIO::loadFromFile(file, r, nullptr, &err) && r.nodes().size() == 2 && r.beams().size() == 1,
                   "Test 201: réouverture depuis un chemin accentué (" + err + ")");
        TEST_CHECK(TSAProjectIO::isTSAFile(file), "Test 201: en-tête lisible depuis un chemin accentué");
        QImage thumb;
        TEST_CHECK(TSAProjectIO::extractThumbnail(file, thumb) && !thumb.isNull(), "Test 201: aperçu lisible depuis un chemin accentué");

        // Chemin ASCII pour les points 2 et 3 : indépendants du point 1
        const QString plain = QDir::currentPath() + "/build/test_tsa_data/test_audit_201.tsa";

        // 2. Chunk GRID de plus de 64 Kio : était écrit avec une longueur 0 (grille perdue)
        TSA::Grid::GridManager grids;
        TSA::Grid::GridDefinition def("Grande grille", TSA::Grid::GridType::Cartesian);
        std::vector<double> xs;
        for (int i = 0; i < 6000; ++i) xs.push_back(0.123456789 * i);
        def.setXPositions(xs);
        def.setYPositions({ 0.0, 5.0 });
        grids.clearAllGrids();
        grids.addGrid(def);
        TEST_CHECK(grids.serializeToJson().size() > 0xFFFF, "Test 201: JSON de grille > 64 Kio (pré-condition)");
        TEST_CHECK(TSAProjectIO::saveToFile(plain, m, &grids, &err), "Test 201: enregistrement d'une grande grille");
        TSA::Grid::GridManager back;
        back.clearAllGrids();
        TEST_CHECK(TSAProjectIO::loadFromFile(plain, r, &back, &err) && back.grids().size() == 1
                       && back.grids().front()->definition().xPositions().size() == xs.size(),
                   "Test 201: grande grille relue en entier");

        // 3. Nom et auteur du projet conservés par ProjectManager::saveProject (enregistrés vides auparavant)
        TSA::Project::ProjectManager pm;
        pm.setProjectName(QString::fromUtf8("Passerelle Analakely"));
        QString qerr;
        TEST_CHECK(pm.saveProject(plain, m, nullptr, QImage(), &qerr), "Test 201: enregistrement par ProjectManager");
        TSA::Project::ProjectManager reopened;
        Model r2;
        TEST_CHECK(reopened.openProject(plain, r2, nullptr, &qerr)
                       && reopened.projectName() == QString::fromUtf8("Passerelle Analakely"),
                   "Test 201: nom du projet relu (" + reopened.projectName().toStdString() + ")");

        // 4. Ouverture refusée : le projet ouvert ne reçoit pas les grilles du fichier rejeté
        {
            TSA::Grid::GridManager rejectedGrids;
            rejectedGrids.clearAllGrids();
            rejectedGrids.addGrid(TSA::Grid::GridDefinition("Rejetée", TSA::Grid::GridType::Cartesian));
            const std::string json = rejectedGrids.serializeToJson();

            std::vector<uint8_t> payload;
            auto appendChunk = [&payload](uint32_t id, uint32_t count, const std::vector<uint8_t>& body) {
                TSA::IO::TSAChunkHeader ch;
                ch.chunkId = id;
                ch.chunkSize = static_cast<uint32_t>(body.size());
                ch.elementCount = count;
                const auto* p = reinterpret_cast<const uint8_t*>(&ch);
                payload.insert(payload.end(), p, p + sizeof(ch));
                payload.insert(payload.end(), body.begin(), body.end());
            };
            std::vector<uint8_t> gridBody { static_cast<uint8_t>(json.size() & 0xFF), static_cast<uint8_t>(json.size() >> 8) };
            gridBody.insert(gridBody.end(), json.begin(), json.end());
            appendChunk(TSA::IO::CHUNK_GRID, 1, gridBody);
            appendChunk(TSA::IO::CHUNK_NODE, 1, {}); // un nœud annoncé, aucun octet : illisible

            TSA::IO::TSAFileHeader header;   // CRC 0 : non contrôlé, le défaut est dans la structure
            header.fileSize = sizeof(header) + payload.size();
            header.uncompressedSize = payload.size();
            const QString broken = QDir::currentPath() + "/build/test_tsa_data/test_audit_201_broken.tsa";
            QFile f(broken);
            TEST_CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "Test 201: écriture du fichier défectueux");
            f.write(reinterpret_cast<const char*>(&header), sizeof(header));
            f.write(reinterpret_cast<const char*>(payload.data()), static_cast<qint64>(payload.size()));
            f.close();

            TSA::Grid::GridManager live;
            live.clearAllGrids();
            live.addGrid(TSA::Grid::GridDefinition("Courante", TSA::Grid::GridType::Cartesian));
            Model current;
            current.addNode(1, 2, 3);
            TEST_CHECK(!TSAProjectIO::loadFromFile(broken, current, &live, &err), "Test 201: fichier défectueux refusé");
            TEST_CHECK(live.grids().size() == 1 && live.grids().front()->definition().name() == "Courante" && current.nodes().size() == 1,
                       "Test 201: projet ouvert intact après un échec d'ouverture");
        }

        std::cout << "[PASS] Test 201: Chemins accentués, chunk > 64 Kio, nom du projet, ouverture atomique (.tsa)" << std::endl;
        ++passed;
    }

    // TEST 506 : propriétés des sections enregistrées, format 2.0 (BUG-110). Fichiers 1.x migrés par l'algorithme
    // historique (propriétés au bit près) ; rayon de congé indépendant du nom après migration ; fonctionnalités
    // obligatoires dans l'en-tête ; fichier non interprétable de manière fiable refusé explicitement ; TSA3D : rayon
    // exporté et relu.
    {
        using TSA::Model::SectionShape;
        auto ishapes = [](const Model& m) {
            std::vector<std::pair<std::string, TSA::Model::Section>> out;
            for (const auto& [id, b] : m.beams())
                if (b.section().shape == SectionShape::IShape) out.emplace_back("B" + std::to_string(id), b.section());
            for (const auto& [id, c] : m.columns())
                if (c.section().shape == SectionShape::IShape) out.emplace_back("C" + std::to_string(id), c.section());
            return out;
        };
        auto bits = [](const TSA::Model::Section& s) {
            return std::vector<double> { s.rootRadius(), s.area(), s.iy(), s.iz(), s.it(), s.wy(), s.wz() };
        };
        // 1. Fichier d'exemple 1.10 : rayon figé = algorithme historique, au bit près ; enregistré en 2.0 puis relu à l'identique
        const QString hangar = QStringLiteral(TSA_SOURCE_DIR) + "/examples/TSA_Demo_Hangar_60x40m.tsa";
        Model old;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadProject(hangar, old), "Test 506: exemple 1.10 ouvert");
        TSA::IO::TSAFileHeader h1;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(hangar.toStdString(), h1) && h1.versionMajor == 1, "Test 506: exemple au format 1.x");
        const auto sections = ishapes(old);
        bool frozen = !sections.empty(), rolled = false;
        for (const auto& [key, s] : sections)
        {
            frozen = frozen && s.filletRadius && *s.filletRadius == s.legacyRootRadius();
            rolled = rolled || s.rootRadius() > 0.0;
        }
        TEST_CHECK(frozen && rolled, "Test 506: migration : rayon de chaque profil en I figé, égal à l'algorithme historique (" << sections.size() << " éléments)");
        const std::string v2 = (QDir::currentPath() + "/test_bug110_v2.tsa").toStdString();
        TSA::IO::TSAFileWriter writer;
        writer.setCompressionEnabled(false);   // charge utile lisible : altérations contrôlées ci-dessous
        std::string err;
        TEST_CHECK(writer.saveToFile(v2, old, nullptr, "P", "A", &err), "Test 506: enregistrement 2.0 (" << err << ")");
        TSA::IO::TSAFileHeader h2;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(v2, h2) && h2.versionMajor >= 2
                       && (h2.requiredFeatures & TSA::IO::FEATURE_SECTION_PROPERTIES) != 0,
                   "Test 506: format ≥ 2.0, propriétés des sections déclarées obligatoires dans l'en-tête");
        Model back;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(v2, back, nullptr, "", nullptr, nullptr, nullptr, &err), "Test 506: relecture 2.0 (" << err << ")");
        const auto again = ishapes(back);
        bool same = again.size() == sections.size();
        for (std::size_t i = 0; same && i < again.size(); ++i) same = again[i].first == sections[i].first && bits(again[i].second) == bits(sections[i].second);
        TEST_CHECK(same, "Test 506: 2.0 relu : rayon, A, Iy, Iz, It, Wy, Wz identiques au bit près");

        // 2. Après migration, le nom ne compte plus : renommer ou modifier une dimension conserve le rayon
        auto rolledBeam = back.beams().begin();
        while (rolledBeam != back.beams().end() && !(rolledBeam->second.section().shape == SectionShape::IShape && rolledBeam->second.section().rootRadius() > 0.0)) ++rolledBeam;
        TEST_CHECK(rolledBeam != back.beams().end(), "Test 506: poutre laminée dans l'exemple");
        auto& beam = *back.getBeam(rolledBeam->first);
        const int rolledId = rolledBeam->first;
        TSA::Model::Section renamed = beam.section();
        const auto before = bits(renamed);
        TEST_CHECK(renamed.shape == SectionShape::IShape && renamed.rootRadius() > 0.0, "Test 506: profil laminé de référence");
        renamed.name = "Poutre P1";
        beam.setSection(renamed);
        TEST_CHECK(bits(beam.section()) == before && beam.section().legacyRootRadius() == 0.0,
                   "Test 506: section renommée : rayon et propriétés inchangés (auparavant rayon 0 : A, I, J réduits)");
        renamed.height += 0.01;
        beam.setSection(renamed);
        TEST_CHECK(beam.section().rootRadius() == before[0], "Test 506: dimension modifiée : rayon conservé");
        const std::string v2b = (QDir::currentPath() + "/test_bug110_v2b.tsa").toStdString();
        Model reread;
        TEST_CHECK(writer.saveToFile(v2b, back, nullptr, "P", "A", &err) && reader.loadFromFile(v2b, reread, nullptr, "", nullptr, nullptr, nullptr, &err)
                       && reread.getBeam(rolledId) && reread.getBeam(rolledId)->section().rootRadius() == before[0],
                   "Test 506: section renommée et modifiée enregistrée puis relue avec son rayon (" << err << ")");

        // 3. Refus explicites : fonctionnalité obligatoire inconnue, SECP absent, empreinte altérée, formules inconnues
        auto readAll = [](const std::string& p) {
            QFile f(QString::fromStdString(p));
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        };
        auto writeAltered = [&](QByteArray bytes, const std::function<void(QByteArray&)>& alter) {
            TSA::IO::TSAFileHeader hdr;
            std::memcpy(&hdr, bytes.constData(), sizeof(hdr));
            QByteArray payload = bytes.mid(static_cast<qsizetype>(hdr.payloadOffset), static_cast<qsizetype>(hdr.fileSize - hdr.payloadOffset));
            alter(payload);
            hdr.checksumCRC32 = TSA::IO::computeCRC32(reinterpret_cast<const uint8_t*>(payload.constData()), static_cast<size_t>(payload.size()));
            bytes.replace(static_cast<qsizetype>(hdr.payloadOffset), payload.size(), payload);
            std::memcpy(bytes.data(), &hdr, sizeof(hdr));
            const std::string out = (QDir::currentPath() + "/test_bug110_altered.tsa").toStdString();
            QFile f(QString::fromStdString(out));
            f.open(QIODevice::WriteOnly);
            f.write(bytes);
            f.close();
            Model m;
            std::string e;
            const bool ok = TSA::IO::TSAFileReader().loadFromFile(out, m, nullptr, "", nullptr, nullptr, nullptr, &e);
            return std::make_pair(ok, e);
        };
        const QByteArray original = readAll(v2);
        {
            QByteArray bytes = original;
            TSA::IO::TSAFileHeader hdr;
            std::memcpy(&hdr, bytes.constData(), sizeof(hdr));
            hdr.requiredFeatures |= 1ULL << 40;
            std::memcpy(bytes.data(), &hdr, sizeof(hdr));
            QFile f(QDir::currentPath() + "/test_bug110_feature.tsa");
            f.open(QIODevice::WriteOnly);
            f.write(bytes);
            f.close();
            Model m;
            std::string e;
            TEST_CHECK(!TSA::IO::TSAFileReader().loadFromFile((QDir::currentPath() + "/test_bug110_feature.tsa").toStdString(), m, nullptr, "", nullptr, nullptr, nullptr, &e)
                           && e.find("fonctionnalités") != std::string::npos && m.beams().empty(),
                       "Test 506: fonctionnalité obligatoire inconnue : refus explicite (" << e << ")");
        }
        const auto noSecp = writeAltered(original, [](QByteArray& p) {
            const qsizetype i = p.indexOf(QByteArray("SECP", 4));
            if (i >= 0) p[i + 3] = 'Q';
        });
        TEST_CHECK(!noSecp.first && noSecp.second.find("SECP") != std::string::npos, "Test 506: chunk SECP absent : refus explicite (" << noSecp.second << ")");
        const auto tampered = writeAltered(original, [](QByteArray& p) {
            qsizetype i = p.indexOf("\"props\":{\"A\":");
            if (i < 0) return;
            for (i += 13; i < p.size(); ++i)
                if (p[i] >= '1' && p[i] <= '8')
                {
                    p[i] = static_cast<char>(p[i] + 1);
                    break;
                }
        });
        TEST_CHECK(!tampered.first && tampered.second.find("recalculée") != std::string::npos,
                   "Test 506: empreinte des propriétés altérée : refus explicite (" << tampered.second << ")");
        const auto futureModel = writeAltered(original, [](QByteArray& p) {
            const qsizetype i = p.indexOf("\"sectionModel\":1");
            if (i >= 0) p[i + 15] = '9';
        });
        TEST_CHECK(!futureModel.first && futureModel.second.find("formules") != std::string::npos,
                   "Test 506: version des formules inconnue : refus explicite (" << futureModel.second << ")");

        // 4. TSA3D : rayon de congé exporté (dimensions.r) et relu ; propriétés inchangées après réimport
        Model renamedModel;
        const int a = renamedModel.addNode(0, 0, 0), b = renamedModel.addNode(6, 0, 0);
        TSA::Model::Section ipe = TSA::Model::Section::ipe(300);
        const int bar = renamedModel.addBar(a, b, ipe, TSA::Model::Material::steelS235());
        TSA::Model::Section p1 = renamedModel.getBeam(bar)->section();
        p1.name = "Poutre P1";
        renamedModel.getBeam(bar)->setSection(p1);
        const auto ref = bits(renamedModel.getBeam(bar)->section());
        const auto ex = TSA::IO::Tsa3d::exportModel(renamedModel, {});
        const QJsonObject dims = ex.document["sections"].toArray().at(0).toObject()["dimensions"].toObject();
        TEST_CHECK(dims.contains("r") && dims["r"].toDouble() == ref[0], "Test 506: TSA3D : dimensions.r exporté (" << dims["r"].toDouble() << ")");
        Model imported;
        const auto im = TSA::IO::Tsa3d::importDocument(ex.document, imported);
        TEST_CHECK(im.ok && !imported.beams().empty() && bits(imported.beams().begin()->second.section()) == ref,
                   "Test 506: TSA3D réimporté : rayon et propriétés identiques (section renommée)");
        std::cout << "[PASS] Test 506: Propriétés des sections enregistrées (format 2.0, migration, refus explicites, TSA3D)" << std::endl;
        ++passed;
    }

    // TEST 574 : blocs d'état du projet (format 3.1, docs/PROJECT_STATE_PLAN.md étape 1) — chunks facultatifs UIST, VSTA,
    // RSLT, DIAG, PMAN : aller-retour exact (binaire compris), sans effet sur l'historique ni sur « modifié », aucun
    // héritage d'un projet précédent, fichier sans bloc identique à avant.
    {
        using Block = Model::ProjectStateBlock;
        const std::string tmpDir = "./build/test_tsa_data/";
        std::filesystem::create_directories(tmpDir);
        const QString file = QString::fromStdString(tmpDir + "test_project_state.tsa");
        const QString bare = QString::fromStdString(tmpDir + "test_project_state_none.tsa");

        Model m;
        m.addNode(0, 0, 0);
        m.markSaved();
        const auto rev = m.revision();
        const size_t undo0 = m.undoManager()->undoCount();
        std::string binary("RSLT\0\x01\xFF\x00 fin", 12);   // octets nuls et non UTF-8 : bloc binaire (résultats)
        const std::array<std::string, 5> data{ R"({"schema":1,"ribbonTab":2})", R"({"schema":1,"render":"materials"})",
                                               binary, R"({"schema":1,"warnings":["é"]})", R"({"schema":1,"libraries":[]})" };
        for (int i = 0; i < 5; ++i) m.setProjectStateBlock(static_cast<Block>(i), data[i]);
        TEST_CHECK(!m.isModified() && m.revision() == rev && m.undoManager()->undoCount() == undo0,
                   "Test 574: blocs d'état sans marque « modifié », révision ni entrée Annuler (D1)");

        std::string err;
        TEST_CHECK(TSAProjectIO::saveToFile(file, m, nullptr, &err), "Test 574: enregistrement avec les 5 blocs (" << err << ")");
        TSA::IO::TSAFileHeader header;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(file.toStdString(), header) && header.versionMajor == 3 && header.versionMinor >= 1,
                   "Test 574: format 3.1 au moins (" << header.versionMajor << "." << header.versionMinor << ")");

        Model r;
        TEST_CHECK(TSAProjectIO::loadFromFile(file, r, nullptr, &err), "Test 574: relecture (" << err << ")");
        bool same = true;
        for (int i = 0; i < 5; ++i) same = same && r.projectStateBlock(static_cast<Block>(i)) == data[i];
        TEST_CHECK(same && r.projectStateBlock(Block::Results).size() == binary.size(), "Test 574: 5 blocs relus à l'identique (binaire compris)");

        // Undo après modification : les blocs ne sont pas restaurés par l'historique (ils ne lui appartiennent pas)
        r.pushUndoState("Nœud");
        r.addNode(1, 0, 0);
        r.setProjectStateBlock(Block::ViewState, "{\"schema\":1,\"render\":\"wire\"}");
        TEST_CHECK(r.undo() && r.projectStateBlock(Block::ViewState) == "{\"schema\":1,\"render\":\"wire\"}",
                   "Test 574: Annuler ne touche pas aux blocs d'état");

        // Fichier sans bloc : rien d'écrit, et la relecture vide les blocs d'un projet précédent
        Model plain;
        plain.addNode(0, 0, 0);
        TEST_CHECK(TSAProjectIO::saveToFile(bare, plain, nullptr, &err), "Test 574: enregistrement sans bloc");
        TEST_CHECK(TSAProjectIO::loadFromFile(bare, r, nullptr, &err), "Test 574: relecture sans bloc dans un modèle déjà rempli");
        bool empty = true;
        for (int i = 0; i < 5; ++i) empty = empty && r.projectStateBlock(static_cast<Block>(i)).empty();
        TEST_CHECK(empty, "Test 574: aucun bloc hérité du projet précédent");

        // clear() (nouveau projet) vide aussi les blocs
        m.clear();
        bool cleared = true;
        for (int i = 0; i < 5; ++i) cleared = cleared && m.projectStateBlock(static_cast<Block>(i)).empty();
        TEST_CHECK(cleared, "Test 574: Model::clear vide les blocs d'état");
        std::cout << "[PASS] Test 574: Blocs d'état du projet (UIST, VSTA, RSLT, DIAG, PMAN) — format 3.1, aller-retour exact" << std::endl;
        ++passed;
    }

    // TEST 578 : messages, erreurs et historique des calculs (bloc DIAG, docs/PROJECT_STATE_PLAN.md étape 5)
    {
        using TSA::Project::ProjectDiagnostics;
        ProjectDiagnostics d;
        TSA::Analysis::AnalysisContext ctx;
        ctx.engineId = "opensees";
        ctx.loadCaseIds = { 1, 2 };
        TSA::Analysis::ResultsModel ok;
        ok.executionMetadata().solverEngine = "OpenSees";
        ok.executionMetadata().solverVersion = "3.8.0";
        ok.executionMetadata().warnings = { "élément court" };
        ok.executionMetadata().preparationMessages = { "voile exclu" };
        ok.setCaseOrComboName("ELU 1");
        for (int i = 0; i < 25; ++i)
            d.recordCalculation(ctx, true, "ok", &ok, "", 120.0 + i, "empreinte" + std::to_string(i), "2026-11-01T10:" + std::to_string(10 + i));
        TEST_CHECK(d.history().size() == ProjectDiagnostics::kMaxRecords && d.history().front().modelFingerprint == "empreinte5" &&
                       d.history().back().engine == "OpenSees 3.8.0" && d.history().back().loadSelection == "ELU 1",
                   "Test 578: historique borné aux 20 derniers calculs (moteur, version, cas)");
        TEST_CHECK(d.lastMessages().size() == 2 && d.history().back().warnings == 2, "Test 578: messages du dernier calcul réussi (préparation, avertissements)");

        // Échec : message d'erreur, journal du moteur limité à ses 500 dernières lignes
        std::string log;
        for (int i = 1; i <= 800; ++i) log += "ligne " + std::to_string(i) + "\n";
        d.recordCalculation(ctx, false, "matrice singulière", nullptr, log, 35.0, "empreinte-x", "2026-11-01T11:00:00Z");
        const std::string& kept = d.lastEngineLog();
        TEST_CHECK(!d.history().back().success && d.lastMessages() == std::vector<std::string>{ "erreur : matrice singulière" } &&
                       kept.rfind("ligne 301\n", 0) == 0 && kept.find("ligne 800") != std::string::npos && kept.find("ligne 300\n") == std::string::npos,
                   "Test 578: échec — message d'erreur, 500 dernières lignes du journal (" << kept.substr(0, 12) << "…)");

        Model v;
        v.addNode(0, 0, 0);
        const auto report = TSA::Standards::ModelValidator::validate(v);
        d.setValidation(report);
        TEST_CHECK(d.validation().size() == report.issues().size(), "Test 578: rapport de validation relevé (" << report.issues().size() << " anomalie(s))");

        ProjectDiagnostics back;
        std::string err;
        TEST_CHECK(back.fromJson(d.toJson(), &err) && back == d, "Test 578: aller-retour JSON identique (" << err << ")");
        ProjectDiagnostics untouched = back;
        TEST_CHECK(!untouched.fromJson(R"({"schema":2,"calculations":[]})", &err) && untouched == back, "Test 578: version inconnue refusée, contenu inchangé");

        // Dans le .tsa (bloc DIAG)
        const QString file = QString::fromStdString(std::string("./build/test_tsa_data/test_diagnostics.tsa"));
        std::filesystem::create_directories("./build/test_tsa_data/");
        v.setProjectStateBlock(Model::ProjectStateBlock::Diagnostics, d.toJson());
        Model r;
        ProjectDiagnostics reread;
        TEST_CHECK(TSAProjectIO::saveToFile(file, v, nullptr, &err) && TSAProjectIO::loadFromFile(file, r, nullptr, &err) &&
                       reread.fromJson(r.projectStateBlock(Model::ProjectStateBlock::Diagnostics), &err) && reread == d,
                   "Test 578: historique et messages relus du .tsa à l'identique (" << err << ")");
        std::cout << "[PASS] Test 578: Messages, erreurs et historique des calculs (bloc DIAG)" << std::endl;
        ++passed;
    }

    // TEST 579 : manifeste du projet (bloc PMAN, docs/PROJECT_STATE_PLAN.md étape 6) — fichiers liés relatifs au .tsa,
    // présents / modifiés / introuvables, projet déplacé sans chemins absolus (D5), bibliothèques, enregistrements.
    {
        using namespace TSA::Project;
        QTemporaryDir tmp;
        TEST_CHECK(tmp.isValid(), "Test 579: dossier temporaire");
        QDir root(tmp.path());
        root.mkpath("etude/sources");
        auto write = [](const QString& path, const QByteArray& data) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
        };
        const QString ifc = root.filePath("etude/sources/batiment.ifc"), note = root.filePath("etude/note.pdf");
        const QString project = root.filePath("etude/projet.tsa");
        TEST_CHECK(write(ifc, "ISO-10303-21; contenu"), "Test 579: fichier IFC (pré-condition)");
        TEST_CHECK(write(note, "%PDF-1.7"), "Test 579: document (pré-condition)");

        ProjectManifest m;
        TEST_CHECK(m.registerLink("import-ifc", ifc.toStdString(), project.toStdString()) &&
                       m.registerLink("attachment", note.toStdString(), project.toStdString()) &&
                       m.registerLink("import-ifc", ifc.toStdString(), project.toStdString()) &&   // même lien : mis à jour
                       !m.registerLink("texture", root.filePath("absente.png").toStdString(), project.toStdString()),
                   "Test 579: liens enregistrés (doublon fusionné, fichier absent refusé)");
        TEST_CHECK(m.links().size() == 2 && m.links()[0].relativePath == "sources/batiment.ifc" && !m.links()[0].sha256.empty(),
                   "Test 579: chemin relatif au .tsa, empreinte relevée (" << m.links()[0].relativePath << ")");
        auto statusOf = [&](const ProjectManifest& pm, const QString& tsa, std::size_t i) { return pm.checkLinks(tsa.toStdString())[i].status; };
        TEST_CHECK(statusOf(m, project, 0) == LinkStatus::Present, "Test 579: fichier présent et inchangé");
        TEST_CHECK(write(ifc, "ISO-10303-21; contenu modifié"), "Test 579: modification (pré-condition)");
        TEST_CHECK(statusOf(m, project, 0) == LinkStatus::Modified, "Test 579: fichier modifié depuis son enregistrement détecté");

        // Projet transmis sans chemins absolus (D5) et déplacé avec ses fichiers : retrouvés par les chemins relatifs
        m.registerLink("import-ifc", ifc.toStdString(), project.toStdString());
        m.updateLinkPaths(project.toStdString(), false);
        TEST_CHECK(m.links()[0].absolutePath.empty() && m.links()[1].absolutePath.empty(), "Test 579: chemins absolus effacés (D5)");
        TEST_CHECK(root.rename("etude", "etude_deplacee"), "Test 579: dossier du projet déplacé (pré-condition)");
        const QString moved = root.filePath("etude_deplacee/projet.tsa");
        TEST_CHECK(statusOf(m, moved, 0) == LinkStatus::Present && statusOf(m, moved, 1) == LinkStatus::Present,
                   "Test 579: projet déplacé — fichiers liés retrouvés par leur chemin relatif");
        QFile::remove(root.filePath("etude_deplacee/note.pdf"));
        TEST_CHECK(statusOf(m, moved, 1) == LinkStatus::Missing, "Test 579: fichier supprimé — introuvable");

        // Bibliothèques requises (références de définitions du modèle)
        Model model;
        TSA::ExtensionSystem::DefinitionReference ref;
        ref.libraryId = "org.exemple.acier";
        ref.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 2, 0);
        ref.definitionId = "steel.s355";
        ref.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
        model.setCalculationSnapshot("material:S355", TSA::ExtensionSystem::MechanicalSnapshot {}, ref);
        m.collectLibraries(model);
        TEST_CHECK(m.libraries().size() == 1 && m.libraries()[0].id == "org.exemple.acier" && m.libraries()[0].version == "1.2.0" &&
                       m.libraries()[0].definitions == std::vector<std::string> { "steel.s355" },
                   "Test 579: bibliothèque requise relevée (" << (m.libraries().empty() ? std::string() : m.libraries()[0].version) << ")");
        TEST_CHECK(m.checkLibraries({ { "org.exemple.acier", "1.2.0" } })[0].sameVersion() &&
                       !m.checkLibraries({ { "org.exemple.acier", "2.0.0" } })[0].sameVersion() &&
                       m.checkLibraries({ { "org.exemple.acier", "2.0.0" } })[0].installed && !m.checkLibraries({})[0].installed,
                   "Test 579: bibliothèque installée, d'une autre version, absente");

        for (int i = 0; i < 55; ++i) m.recordSave({ "2026-11-01T12:" + std::to_string(i), "Ingénieur", "TSA 0.1.0", "3.1" });
        m.setProperties({ "Bâtiment R+4", "Commune", "AFF-2026-12", "APD" });
        TEST_CHECK(m.saves().size() == ProjectManifest::kMaxSaves && m.saves().front().timestamp == "2026-11-01T12:5",
                   "Test 579: historique borné aux 50 derniers enregistrements");

        ProjectManifest back;
        std::string err;
        TEST_CHECK(back.fromJson(m.toJson(), &err) && back == m, "Test 579: aller-retour JSON identique (" << err << ")");
        ProjectManifest untouched = back;
        TEST_CHECK(!untouched.fromJson(R"({"schema":3})", &err) && untouched == back, "Test 579: version inconnue refusée, contenu inchangé");
        model.setProjectStateBlock(Model::ProjectStateBlock::Manifest, m.toJson());
        const QString tsa = root.filePath("manifeste.tsa");
        Model reread;
        ProjectManifest fromFile;
        TEST_CHECK(TSAProjectIO::saveToFile(tsa, model, nullptr, &err) && TSAProjectIO::loadFromFile(tsa, reread, nullptr, &err) &&
                       fromFile.fromJson(reread.projectStateBlock(Model::ProjectStateBlock::Manifest), &err) && fromFile == m,
                   "Test 579: manifeste relu du .tsa à l'identique (" << err << ")");
        std::cout << "[PASS] Test 579: Manifeste du projet (bloc PMAN) — fichiers liés, bibliothèques, enregistrements" << std::endl;
        ++passed;
    }


    // TEST 588 (BUG-154) : .tsa à références pendantes — refusé à l'ouverture (projet ouvert intact), aucun plantage au calcul.
    // Avant : un fichier dont une barre désigne un nœud absent s'ouvrait, puis le calcul faisait une violation d'accès.
    {
        using namespace TSA::Analysis;
        QTemporaryDir dir;
        std::string err;
        // Fichier fabriqué : poutre dont un nœud a été retiré de l'instantané
        Model source;
        const int a = source.addNode(0, 0, 0), b = source.addNode(5, 0, 0);
        source.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        auto snap = source.createSnapshot("test");
        snap.nodes.erase(b);
        Model broken;
        broken.restoreSnapshot(snap);
        const QString file = dir.filePath("pendant.tsa");
        TEST_CHECK(TSAProjectIO::saveToFile(file, broken, nullptr, &err), "Test 588: fichier fabriqué écrit (" << err << ")");
        Model victim;
        victim.addNode(1, 2, 3);
        err.clear();
        const bool opened = TSAProjectIO::loadFromFile(file, victim, nullptr, &err);
        TEST_CHECK(!opened && err.find("nœud") != std::string::npos && err.find("poutre") != std::string::npos,
                   "Test 588: fichier à référence pendante refusé avec un message précis (" << err << ")");
        TEST_CHECK(victim.nodes().size() == 1 && victim.beams().empty(), "Test 588: projet ouvert intact après le refus");

        // Fichiers du fuzzing de l'audit (docs/audit/2026-10-31/preuves/fuzz) : ouverture refusée, ou calcul sans plantage
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        for (const char* name : { "analyse_ACCESS_VIOLATION_00216.tsa", "analyse_STACK_BUFFER_OVERRUN_00245.tsa" })
        {
            const QString path = QStringLiteral(TSA_SOURCE_DIR "/docs/audit/2026-10-31/preuves/fuzz/") + QString::fromLatin1(name);
            if (!QFile::exists(path))
            {
                std::cout << "[SKIP] Test 588: " << name << " absent" << std::endl;
                continue;
            }
            Model fuzzed;
            TSA::Grid::GridManager gm;
            err.clear();
            if (!TSAProjectIO::loadFromFile(path, fuzzed, &gm, &err))
            {
                std::cout << "[INFO] Test 588: " << name << " refusé : " << err << std::endl;
                continue;
            }
            AnalysisManager mgr(reg);
            AnalysisContext c;
            c.engineId = "opensees";
            const auto prepared = mgr.prepare(fuzzed, &gm, c);
            const auto run = mgr.run(c, prepared);
            std::cout << "[INFO] Test 588: " << name << " ouvert ; calcul " << (run.success ? "réussi" : "refusé : " + run.message.substr(0, 160))
                      << std::endl;
        }
        std::cout << "[PASS] Test 588: Fichier à références pendantes refusé, aucun plantage au calcul" << std::endl;
        ++passed;
    }


    // TEST 592 (BUG-157) : copies de récupération — écrites hors du projet, verrouillées tant que le document est ouvert,
    // proposées après une fermeture brutale (verrou d'un processus disparu), relues à l'identique, effacées
    {
        using namespace TSA::Project;
        QTemporaryDir dir;
        const RecoveryStore store(dir.filePath("recovery"));
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0);
        m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const QString key = QStringLiteral("doc-1");
        auto held = store.lock(key);
        QString err;
        TEST_CHECK(held && store.write(key, m, nullptr, QStringLiteral("C:/Projets/Pont.tsa"), QStringLiteral("Pont"), &err),
                   "Test 592: copie de récupération écrite (" << err.toStdString() << ")");
        TEST_CHECK(store.orphans().empty(), "Test 592: document encore ouvert (verrou tenu) : copie non proposée");
        held.reset();   // fermeture du document sans effacement (cas d'un arrêt) : verrou libéré
        auto orphans = store.orphans();
        TEST_CHECK(orphans.size() == 1 && orphans.front().originalPath == "C:/Projets/Pont.tsa" && orphans.front().title == "Pont" &&
                       orphans.front().savedAt.isValid(),
                   "Test 592: copie orpheline proposée avec son fichier d'origine");
        Model back;
        TEST_CHECK(TSAProjectIO::loadFromFile(orphans.front().file, back) && back.nodes().size() == 2 && back.beams().size() == 1,
                   "Test 592: copie relue à l'identique");
        store.remove(key);
        TEST_CHECK(store.orphans().empty() && !QFile::exists(orphans.front().file), "Test 592: copie effacée");

        // Fermeture brutale : verrou laissé par un processus disparu → copie proposée ; processus vivant → non proposée
        const QString key2 = QStringLiteral("doc-2");
        TEST_CHECK(store.write(key2, m, nullptr, QString(), QStringLiteral("Sans titre"), &err), "Test 592: deuxième copie écrite");
        {
            QFile stale(QDir(store.directory()).filePath(key2 + ".lock"));
            TEST_CHECK(stale.open(QIODevice::WriteOnly), "Test 592: verrou périmé simulé");
            stale.write(QStringLiteral("2147483600\nTSA\n%1\n").arg(QSysInfo::machineHostName()).toUtf8());
        }
        TEST_CHECK(store.orphans().size() == 1 && store.orphans().front().originalPath.isEmpty(),
                   "Test 592: verrou d'un processus disparu : copie proposée (projet jamais enregistré)");
        auto live = store.lock(QStringLiteral("doc-3"));
        TEST_CHECK(live && store.write(QStringLiteral("doc-3"), m, nullptr, QString(), QString(), &err) && store.orphans().size() == 1,
                   "Test 592: copie d'un document ouvert (processus vivant) jamais proposée");

        // Projet récupéré rattaché à son fichier d'origine
        ProjectManager pm;
        pm.adoptFilePath(QStringLiteral("C:/Projets/Pont.tsa"));
        TEST_CHECK(pm.hasFilePath() && pm.currentFilePath() == "C:/Projets/Pont.tsa", "Test 592: projet récupéré rattaché à son fichier");
        TEST_CHECK(RecoveryStore::intervalMinutes() >= 0, "Test 592: intervalle de sauvegarde automatique lu");
        std::cout << "[PASS] Test 592: Copies de récupération (sauvegarde automatique, fermeture brutale)" << std::endl;
        ++passed;
    }

    // TEST 599 (BUG-167, BUG-166) : projets protégés par mot de passe — AES-256-GCM authentifié (clé PBKDF2), mot de passe
    // demandé / refusé, contenu illisible sans lui, modification détectée, anciens fichiers XOR toujours lus, copie de
    // récupération chiffrée ; somme de contrôle toujours vérifiée (un CRC à 0 ne désactive plus le contrôle)
    {
        using namespace TSA::IO;
        using OpenStatus = TSAProjectIO::OpenStatus;
        TEST_CHECK(Crypto::available(), "Test 599: chiffrement disponible (" << Crypto::backendName() << ")");

        // a) Bloc chiffré : aller-retour, mauvais mot de passe, altérations, aléa (sel et nonce)
        const std::string secret = "Données de calcul confidentielles — 42 kN";
        const auto* plain = reinterpret_cast<const uint8_t*>(secret.data());
        std::vector<uint8_t> block, block2, back;
        TEST_CHECK(Crypto::encrypt(plain, secret.size(), "correct horse", block, Crypto::kMinIterations) == Crypto::Status::Ok
                       && block.size() == secret.size() + Crypto::kHeaderSize + Crypto::kTagSize && Crypto::looksEncrypted(block.data(), block.size()),
                   "Test 599: bloc chiffré (en-tête, contenu, étiquette)");
        TEST_CHECK(Crypto::decrypt(block.data(), block.size(), "correct horse", back) == Crypto::Status::Ok
                       && std::string(back.begin(), back.end()) == secret,
                   "Test 599: déchiffré à l'identique");
        back.clear();
        TEST_CHECK(Crypto::decrypt(block.data(), block.size(), "correct horsE", back) == Crypto::Status::WrongPassword && back.empty(),
                   "Test 599: mauvais mot de passe refusé, rien rendu");
        TEST_CHECK(std::search(block.begin(), block.end(), secret.begin(), secret.end()) == block.end(), "Test 599: contenu absent du bloc");
        Crypto::encrypt(plain, secret.size(), "correct horse", block2, Crypto::kMinIterations);
        TEST_CHECK(block2 != block, "Test 599: deux chiffrements du même contenu diffèrent (sel et nonce aléatoires)");
        auto altered = block;
        altered[Crypto::kHeaderSize + 3] ^= 0x01;
        TEST_CHECK(Crypto::decrypt(altered.data(), altered.size(), "correct horse", back) == Crypto::Status::WrongPassword,
                   "Test 599: un bit du contenu modifié — refusé (authentifié)");
        altered = block;
        altered[12] ^= 0x01;   // sel : paramètre authentifié
        TEST_CHECK(Crypto::decrypt(altered.data(), altered.size(), "correct horse", back) != Crypto::Status::Ok, "Test 599: sel modifié — refusé");
        altered = block;
        altered[8] = 0xFF; altered[9] = 0xFF; altered[10] = 0xFF; altered[11] = 0xFF;   // itérations démesurées
        TEST_CHECK(Crypto::decrypt(altered.data(), altered.size(), "correct horse", back) == Crypto::Status::Corrupt,
                   "Test 599: itérations hors bornes — refusé sans calcul");
        TEST_CHECK(Crypto::decrypt(block.data(), 20, "correct horse", back) == Crypto::Status::Corrupt, "Test 599: bloc tronqué refusé");

        // b) Projet protégé : en-tête sans nom, contenu illisible, mot de passe demandé puis refusé puis accepté
        QTemporaryDir dir;
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(7, 0, 0);
        m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const QString file = dir.filePath("Protege.tsa");
        const QString password = QStringLiteral("Pont-%1té-2026").arg(QChar(0x00E9));   // « é » précomposé
        QString qerr;
        TEST_CHECK(TSAProjectIO::saveProtectedProject(file, m, nullptr, QStringLiteral("Pont Secret XYZ"), QStringLiteral("Bureau Alpha"), password,
                                                      QImage(), &qerr),
                   "Test 599: projet protégé enregistré (" << qerr.toStdString() << ")");
        TSAFileHeader h;
        TEST_CHECK(TSAFileReader::readHeader(file.toStdString(), h) && (h.flags & FLAG_ENCRYPTED) && (h.flags & FLAG_ENCRYPTED_AEAD)
                       && (h.requiredFeatures & FEATURE_ENCRYPTION_AEAD) && h.projectName[0] == '\0' && h.author[0] == '\0'
                       && !(h.flags & FLAG_HAS_PREVIEW_BLOCK),
                   "Test 599: en-tête — chiffrement annoncé, ni nom ni auteur en clair, aucun aperçu");
        QFile raw(file);
        raw.open(QIODevice::ReadOnly);
        QByteArray bytes = raw.readAll();
        raw.close();
        TEST_CHECK(!bytes.contains("Pont Secret XYZ") && !bytes.contains("Bureau Alpha") && !bytes.contains("IPE"),
                   "Test 599: nom, auteur et sections absents du fichier");
        TEST_CHECK(TSAProjectIO::isPasswordProtected(file), "Test 599: projet reconnu protégé sans le déchiffrer");
        Model r1;
        TEST_CHECK(TSAProjectIO::loadProtectedProject(file, r1, nullptr, QString()) == OpenStatus::PasswordRequired && r1.nodes().empty(),
                   "Test 599: sans mot de passe — demandé, rien chargé");
        TEST_CHECK(TSAProjectIO::loadProtectedProject(file, r1, nullptr, QStringLiteral("pont-été-2026")) == OpenStatus::WrongPassword && r1.nodes().empty(),
                   "Test 599: mauvais mot de passe — refusé, rien chargé");
        QString name;
        const QString decomposed = QStringLiteral("Pont-e%1t%2-2026").arg(QChar(0x0301)).arg(QStringLiteral("e") + QChar(0x0301));
        TEST_CHECK(TSAProjectIO::loadProtectedProject(file, r1, nullptr, decomposed, &name) == OpenStatus::Ok && r1.nodes().size() == 2
                       && r1.beams().size() == 1 && name == "Pont Secret XYZ",
                   "Test 599: bon mot de passe (saisi en caractères décomposés, normalisé NFC) — projet relu");
        Model r0;
        TEST_CHECK(!TSAProjectIO::loadProject(file, r0) && r0.nodes().empty(), "Test 599: ancienne API sans mot de passe — refus");
        QImage thumb;
        TEST_CHECK(!TSAProjectIO::extractThumbnail(file, thumb), "Test 599: aucune miniature lue sans mot de passe");

        // c) Altérations du fichier : contenu chiffré modifié (CRC recalculé) → refusé ; CRC faux ou nul → refusé (BUG-166)
        auto rewrite = [&](const QString& path, const QByteArray& data) {
            QFile f(path);
            f.open(QIODevice::WriteOnly | QIODevice::Truncate);
            f.write(data);
            f.close();
        };
        auto withCrc = [&](QByteArray data, bool recompute, uint32_t forced = 0) {
            TSAFileHeader hd;
            std::memcpy(&hd, data.constData(), sizeof(hd));
            hd.checksumCRC32 = recompute ? computeCRC32(reinterpret_cast<const uint8_t*>(data.constData()) + hd.payloadOffset,
                                                        static_cast<size_t>(hd.fileSize - hd.payloadOffset))
                                         : forced;
            std::memcpy(data.data(), &hd, sizeof(hd));
            return data;
        };
        QByteArray tampered = bytes;
        tampered[int(h.payloadOffset) + 60] = char(tampered[int(h.payloadOffset) + 60] ^ 0x10);
        const QString bad = dir.filePath("Altere.tsa");
        rewrite(bad, withCrc(tampered, true));
        Model r2;
        TEST_CHECK(TSAProjectIO::loadProtectedProject(bad, r2, nullptr, password) == OpenStatus::WrongPassword && r2.nodes().empty(),
                   "Test 599: contenu chiffré modifié (CRC recalculé) — refusé par l'authentification");
        rewrite(bad, withCrc(tampered, false, h.checksumCRC32));
        QString crcErr;
        TEST_CHECK(TSAProjectIO::loadProtectedProject(bad, r2, nullptr, password, nullptr, nullptr, nullptr, &crcErr) == OpenStatus::Failed
                       && crcErr.contains("CRC32"),
                   "Test 599: somme de contrôle fausse — refusée");
        // CRC nul (BUG-166) : fichier en clair altéré, champ mis à 0 pour contourner le contrôle
        const QString plainFile = dir.filePath("Clair.tsa");
        TEST_CHECK(TSAProjectIO::saveProject(plainFile, m), "Test 599: projet en clair enregistré");
        QFile pf(plainFile);
        pf.open(QIODevice::ReadOnly);
        QByteArray plainBytes = pf.readAll();
        pf.close();
        TEST_CHECK(!TSAProjectIO::isPasswordProtected(plainFile), "Test 599: projet en clair non protégé");
        TSAFileHeader hp;
        std::memcpy(&hp, plainBytes.constData(), sizeof(hp));
        TEST_CHECK(!(hp.requiredFeatures & FEATURE_ENCRYPTION_AEAD), "Test 599: projet en clair lisible par les versions 3.0 / 3.1 (aucun bit de chiffrement)");
        QByteArray zeroCrc = plainBytes;
        zeroCrc[int(hp.payloadOffset) + 40] = char(zeroCrc[int(hp.payloadOffset) + 40] ^ 0x5A);
        rewrite(bad, withCrc(zeroCrc, false, 0));
        crcErr.clear();
        TEST_CHECK(TSAProjectIO::loadProtectedProject(bad, r2, nullptr, QString(), nullptr, nullptr, nullptr, &crcErr) == OpenStatus::Failed
                       && crcErr.contains("CRC32") && r2.nodes().empty(),
                   "Test 599: CRC mis à 0 — contrôle toujours fait, fichier altéré refusé (BUG-166)");

        // d) Ancien fichier « chiffré » par XOR (antérieur à 3.2) : toujours lu avec son mot de passe, jamais réécrit ainsi
        {
            TSAFileHeader hl;
            std::memcpy(&hl, plainBytes.constData(), sizeof(hl));
            const auto* payload = reinterpret_cast<const uint8_t*>(plainBytes.constData()) + hl.payloadOffset;
            const size_t payloadSize = static_cast<size_t>(hl.fileSize - hl.payloadOffset);
            std::vector<uint8_t> xored;
            StandardEncryptionProvider().encrypt(payload, payloadSize, xored, "ancien");
            hl.flags = (hl.flags | FLAG_ENCRYPTED) & ~uint32_t(FLAG_HAS_PREVIEW_BLOCK);
            hl.fileSize = hl.payloadOffset + xored.size();
            hl.checksumCRC32 = computeCRC32(xored.data(), xored.size());
            QByteArray legacy(reinterpret_cast<const char*>(&hl), sizeof(hl));
            legacy.append(reinterpret_cast<const char*>(xored.data()), static_cast<qsizetype>(xored.size()));
            const QString legacyFile = dir.filePath("AncienXor.tsa");
            rewrite(legacyFile, legacy);
            Model r3;
            TEST_CHECK(TSAProjectIO::loadProtectedProject(legacyFile, r3, nullptr, QString()) == OpenStatus::PasswordRequired,
                       "Test 599: ancien fichier XOR — mot de passe demandé");
            TEST_CHECK(TSAProjectIO::loadProtectedProject(legacyFile, r3, nullptr, QStringLiteral("faux")) == OpenStatus::WrongPassword,
                       "Test 599: ancien fichier XOR — mauvais mot de passe refusé");
            TEST_CHECK(TSAProjectIO::loadProtectedProject(legacyFile, r3, nullptr, QStringLiteral("ancien")) == OpenStatus::Ok && r3.nodes().size() == 2,
                       "Test 599: ancien fichier XOR — relu avec son mot de passe");
        }

        // e) Gestionnaire du projet : mot de passe retenu, réenregistré protégé, protection retirée
        {
            using TSA::Project::ProjectManager;
            ProjectManager pm;
            Model r4;
            TEST_CHECK(pm.openProject(file, r4, nullptr, password, nullptr) == OpenStatus::Ok && pm.hasPassword(),
                       "Test 599: projet protégé ouvert — mot de passe retenu pour la session");
            const QString again = dir.filePath("Encore.tsa");
            TEST_CHECK(pm.saveProject(again, r4, nullptr) && TSAProjectIO::isPasswordProtected(again), "Test 599: réenregistré protégé");
            pm.setPassword(QString());
            TEST_CHECK(pm.saveProject(again, r4, nullptr) && !TSAProjectIO::isPasswordProtected(again), "Test 599: protection retirée — fichier en clair");
            Model r5;
            TEST_CHECK(pm.openProject(again, r5, nullptr, QStringLiteral("inutile"), nullptr) == OpenStatus::Ok && !pm.hasPassword(),
                       "Test 599: projet en clair — aucun mot de passe retenu");
        }

        // f) Copie de récupération d'un projet protégé : chiffrée, jamais en clair
        {
            const TSA::Project::RecoveryStore store(dir.filePath("recovery"));
            QString err;
            const QString copy = QDir(store.directory()).filePath("doc-p.tsa");
            TEST_CHECK(store.write(QStringLiteral("doc-p"), m, nullptr, file, QStringLiteral("Pont Secret XYZ"), &err, password, QStringLiteral("Bureau Alpha"))
                           && TSAProjectIO::isPasswordProtected(copy),
                       "Test 599: copie de récupération chiffrée (" << err.toStdString() << ")");
            QFile sidecar(QDir(store.directory()).filePath("doc-p.json"));
            sidecar.open(QIODevice::ReadOnly);
            TEST_CHECK(!sidecar.readAll().contains("Pont Secret XYZ"), "Test 599: fiche de récupération sans le nom du projet protégé");
            const auto orphans = store.orphans();
            TEST_CHECK(orphans.size() == 1 && orphans.front().title.isEmpty() && orphans.front().originalPath == file,
                       "Test 599: copie proposée d'après le fichier d'origine");
            Model r6;
            QString nameBack, authorBack;
            TEST_CHECK(TSAProjectIO::loadProtectedProject(copy, r6, nullptr, password, &nameBack, &authorBack) == OpenStatus::Ok
                           && nameBack == "Pont Secret XYZ" && authorBack == "Bureau Alpha",
                       "Test 599: nom et auteur retrouvés dans la copie récupérée");
            // Projet non protégé : nom et auteur gardés aussi (ils étaient perdus à la récupération)
            Model r7;
            TEST_CHECK(store.write(QStringLiteral("doc-c"), m, nullptr, QString(), QStringLiteral("Pont"), &err, QString(), QStringLiteral("Bureau"))
                           && TSAProjectIO::loadProject(QDir(store.directory()).filePath("doc-c.tsa"), r7, nullptr, &nameBack, &authorBack)
                           && nameBack == "Pont" && authorBack == "Bureau",
                       "Test 599: copie en clair — nom et auteur gardés");
        }
        std::cout << "[PASS] Test 599: Projets protégés par mot de passe (AES-256-GCM) ; CRC toujours vérifié" << std::endl;
        ++passed;
    }

    return true;
}
