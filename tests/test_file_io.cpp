#include "test_common.h"
#include "Analysis/Engine/AnalysisContext.h"
#include <QJsonDocument>

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
            QString manifestPath = "e:/Book/Dev/TSA/Extensions/TSALib/manifest.json";
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
            auto valRes = validator.validateExtensionDirectory("e:/Book/Dev/TSA/Extensions/TSALib");
            TEST_CHECK(valRes.valid, "Subtest 40.2: Extensions/TSALib directory validates without errors");

            std::cout << "  [PASS] Subtest 40.2: LibraryValidator Full Directory Validation Verified" << std::endl;
        }

        // 40.3: Standards Directory & Reference Specifications
        {
            QString standardsDir = "e:/Book/Dev/TSA/Extensions/TSALib/Standards";
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
            libMgr.addSearchPath("e:/Book/Dev/TSA/Extensions");
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
        TEST_CHECK(header.magic == TSA::IO::TSA_FILE_MAGIC, "Subtest 45.2: Magic TSAF valide");
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
        int mlId = lm.addMemberLoad(MemberLoad::trapezoidal(0, b1, 1, -5.0, -8.0, 0.1, 0.9, LoadDirection::GlobalZ,
                                                             LoadCoordSystem::Global, true, "q_trap"));
        MemberLoad colLoad = MemberLoad::uniform(0, c1, customCase, 2.0, LoadDirection::GlobalX, LoadCoordSystem::Global,
                                                 "vent_poteau", MemberTargetType::Column);
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

    return true;
}
