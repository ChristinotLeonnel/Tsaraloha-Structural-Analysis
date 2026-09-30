#include "test_common.h"
#include "Model/SupportDefinition.h"
#include "Model/Node.h"
#include "Geometry/SupportGeometry.h"
#include "Analysis/CalculationSnapshot.h"
#include "Analysis/OpenSeesAnalysisBuilder.h"

#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>

bool test_SupportDefinitionPresets()
{
    std::cout << "[Test 70] SupportDefinition Presets & 6 DOFs ... ";

    // Free
    auto freeSupp = TSA::Model::SupportDefinition::free();
    TEST_CHECK(freeSupp.isFree(), "freeSupp should be free");
    TEST_CHECK(!freeSupp.isFixed(), "freeSupp is not fixed");
    TEST_CHECK(freeSupp.supportType() == TSA::Model::SupportType::Free, "supportType is Free");

    // Fixed
    auto fixSupp = TSA::Model::SupportDefinition::fixed();
    TEST_CHECK(fixSupp.isFixed(), "fixSupp should be fixed");
    TEST_CHECK(fixSupp.tx() == TSA::Model::DOFState::Fixed, "tx fixed");
    TEST_CHECK(fixSupp.ty() == TSA::Model::DOFState::Fixed, "ty fixed");
    TEST_CHECK(fixSupp.tz() == TSA::Model::DOFState::Fixed, "tz fixed");
    TEST_CHECK(fixSupp.rx() == TSA::Model::DOFState::Fixed, "rx fixed");
    TEST_CHECK(fixSupp.ry() == TSA::Model::DOFState::Fixed, "ry fixed");
    TEST_CHECK(fixSupp.rz() == TSA::Model::DOFState::Fixed, "rz fixed");
    TEST_CHECK(fixSupp.supportType() == TSA::Model::SupportType::Fixed, "supportType is Fixed");

    // Pinned
    auto pinSupp = TSA::Model::SupportDefinition::pinned();
    TEST_CHECK(pinSupp.isPinned(), "pinSupp should be pinned");
    TEST_CHECK(pinSupp.tx() == TSA::Model::DOFState::Fixed, "tx fixed");
    TEST_CHECK(pinSupp.ty() == TSA::Model::DOFState::Fixed, "ty fixed");
    TEST_CHECK(pinSupp.tz() == TSA::Model::DOFState::Fixed, "tz fixed");
    TEST_CHECK(pinSupp.rx() == TSA::Model::DOFState::Free, "rx free");
    TEST_CHECK(pinSupp.ry() == TSA::Model::DOFState::Free, "ry free");
    TEST_CHECK(pinSupp.rz() == TSA::Model::DOFState::Free, "rz free");

    // Roller
    auto rollSupp = TSA::Model::SupportDefinition::roller();
    TEST_CHECK(rollSupp.isRoller(), "rollSupp should be roller");
    TEST_CHECK(rollSupp.tz() == TSA::Model::DOFState::Fixed, "tz fixed for roller");
    TEST_CHECK(rollSupp.tx() == TSA::Model::DOFState::Free, "tx free for roller");

    // Elastic / Springs
    auto elastSupp = TSA::Model::SupportDefinition::elastic(5000.0, 5000.0, 10000.0, 100.0, 100.0, 200.0);
    TEST_CHECK(elastSupp.hasSprings(), "elastSupp should have springs");
    TEST_CHECK(elastSupp.tx() == TSA::Model::DOFState::Spring, "tx spring");
    TEST_CHECK(approxEqual(elastSupp.kx(), 5000.0), "kx stiffness");
    TEST_CHECK(approxEqual(elastSupp.kz(), 10000.0), "kz stiffness");
    TEST_CHECK(approxEqual(elastSupp.krz(), 200.0), "krz rotational stiffness");

    // Custom
    auto custSupp = TSA::Model::SupportDefinition::custom(
        TSA::Model::DOFState::Fixed, TSA::Model::DOFState::Free, TSA::Model::DOFState::Spring,
        TSA::Model::DOFState::Free, TSA::Model::DOFState::Fixed, TSA::Model::DOFState::Free
    );
    custSupp.setKz(1234.5);
    TEST_CHECK(custSupp.tx() == TSA::Model::DOFState::Fixed, "custom tx");
    TEST_CHECK(custSupp.ty() == TSA::Model::DOFState::Free, "custom ty");
    TEST_CHECK(custSupp.tz() == TSA::Model::DOFState::Spring, "custom tz");
    TEST_CHECK(approxEqual(custSupp.kz(), 1234.5), "custom kz");
    TEST_CHECK(!custSupp.dofSummary().empty(), "dofSummary not empty");

    std::cout << "PASSED" << std::endl;
    return true;
}

bool test_NodeSupportIntegration()
{
    std::cout << "[Test 71] Node Support Integration & Legacy Compatibility ... ";

    TSA::Model::Node node(1, 10.0, 20.0, 0.0, "L0", "N1");
    TEST_CHECK(node.support().isFree(), "Node default support is free");
    TEST_CHECK(node.supportType() == TSA::Model::SupportType::Free, "Legacy supportType is Free");

    // Set fixed support via rich definition
    node.setSupport(TSA::Model::SupportDefinition::fixed());
    TEST_CHECK(node.support().isFixed(), "Node support is fixed");
    TEST_CHECK(node.supportType() == TSA::Model::SupportType::Fixed, "Legacy supportType reports Fixed");

    // Set pinned support via legacy setter
    node.setSupportType(TSA::Model::SupportType::Pinned);
    TEST_CHECK(node.support().isPinned(), "Node support became pinned");
    TEST_CHECK(node.support().tx() == TSA::Model::DOFState::Fixed, "tx is fixed");
    TEST_CHECK(node.support().rx() == TSA::Model::DOFState::Free, "rx is free");

    // Equality operators
    TSA::Model::SupportDefinition s1 = TSA::Model::SupportDefinition::fixed();
    TSA::Model::SupportDefinition s2 = TSA::Model::SupportDefinition::fixed();
    TSA::Model::SupportDefinition s3 = TSA::Model::SupportDefinition::pinned();
    TEST_CHECK(s1 == s2, "s1 == s2");
    TEST_CHECK(s1 != s3, "s1 != s3");

    std::cout << "PASSED" << std::endl;
    return true;
}

bool test_Support3DGeometry()
{
    std::cout << "[Test 72] Support 3D OpenCASCADE Geometry Generation ... ";

    gp_Pnt p0(0.0, 0.0, 0.0);

    // Fixed support shape
    TopoDS_Shape fixedShape = TSA::Geometry::SupportGeometry::createFixedSupportShape(p0, gp_Dir(0, 0, 1), 0.4);
    TEST_CHECK(!fixedShape.IsNull(), "fixedShape must not be null");

    // Count faces in fixedShape
    int faceCount = 0;
    for (TopExp_Explorer exp(fixedShape, TopAbs_FACE); exp.More(); exp.Next())
    {
        ++faceCount;
    }
    TEST_CHECK(faceCount >= 1, "fixedShape should contain at least 1 face");

    // Pinned support shape
    TopoDS_Shape pinnedShape = TSA::Geometry::SupportGeometry::createPinnedSupportShape(p0, gp_Dir(0, 0, 1), 0.4);
    TEST_CHECK(!pinnedShape.IsNull(), "pinnedShape must not be null");
    int pinnedFaces = 0;
    for (TopExp_Explorer exp(pinnedShape, TopAbs_FACE); exp.More(); exp.Next())
    {
        ++pinnedFaces;
    }
    TEST_CHECK(pinnedFaces >= 4, "pinnedShape should contain pyramid and plate faces");

    // Roller support shape
    TopoDS_Shape rollerShape = TSA::Geometry::SupportGeometry::createRollerSupportShape(p0, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0), 0.4);
    TEST_CHECK(!rollerShape.IsNull(), "rollerShape must not be null");

    // Spring support shape
    TopoDS_Shape springShape = TSA::Geometry::SupportGeometry::createSpringShape(p0, gp_Dir(0, 0, 1), 0.4, 4);
    TEST_CHECK(!springShape.IsNull(), "springShape must not be null");
    int edgeCount = 0;
    for (TopExp_Explorer exp(springShape, TopAbs_EDGE); exp.More(); exp.Next())
    {
        ++edgeCount;
    }
    TEST_CHECK(edgeCount >= 10, "springShape should contain discretized helical edges");

    // Rotational spring shape
    TopoDS_Shape rotSpring = TSA::Geometry::SupportGeometry::createRotationalSpringShape(p0, gp_Dir(0, 0, 1), 0.4);
    TEST_CHECK(!rotSpring.IsNull(), "rotSpring must not be null");

    // Node-based wrapper
    TSA::Model::Node n(42, 5.0, 5.0, 0.0, "L0", "N42");
    n.setSupport(TSA::Model::SupportDefinition::fixed());
    TopoDS_Shape nodeSuppShape = TSA::Geometry::SupportGeometry::createSupportShape(n, 0.35);
    TEST_CHECK(!nodeSuppShape.IsNull(), "nodeSuppShape must not be null");

    std::cout << "PASSED" << std::endl;
    return true;
}

bool test_SupportFileSerialization()
{
    std::cout << "[Test 73] Support File Serialization & Roundtrip (.tsa) ... ";

    QString tempFile = QDir::temp().filePath("tsa_test_support_serialization.tsa");
    if (QFile::exists(tempFile)) QFile::remove(tempFile);

    TSA::Model::Model modelSave;
    modelSave.addNodeWithId(1, 0.0, 0.0, 0.0, "L0", "Base1");
    modelSave.getNode(1)->setSupport(TSA::Model::SupportDefinition::fixed());

    modelSave.addNodeWithId(2, 5.0, 0.0, 0.0, "L0", "Base2");
    modelSave.getNode(2)->setSupport(TSA::Model::SupportDefinition::pinned());

    modelSave.addNodeWithId(3, 10.0, 0.0, 0.0, "L0", "Base3");
    auto rollSupp = TSA::Model::SupportDefinition::roller(0.0, 1.0, 0.0);
    modelSave.getNode(3)->setSupport(rollSupp);

    modelSave.addNodeWithId(4, 15.0, 0.0, 0.0, "L0", "Base4");
    auto elastSupp = TSA::Model::SupportDefinition::elastic(8500.0, 9500.0, 25000.0, 150.0, 250.0, 350.0);
    modelSave.getNode(4)->setSupport(elastSupp);

    QString errMsg;
    bool saved = TSAProjectIO::saveProject(tempFile, modelSave, nullptr, "Test Support Project", "TSA Tester", true, QImage(), &errMsg);
    TEST_CHECK(saved, ("saveProject failed: " + errMsg.toStdString()).c_str());

    TSA::Model::Model modelLoad;
    bool loaded = TSAProjectIO::loadProject(tempFile, modelLoad, nullptr, nullptr, nullptr, nullptr, &errMsg);
    TEST_CHECK(loaded, ("loadProject failed: " + errMsg.toStdString()).c_str());

    const auto* ln1 = modelLoad.getNode(1);
    TEST_CHECK(ln1 != nullptr, "Node 1 loaded");
    TEST_CHECK(ln1->support().isFixed(), "Node 1 support is fixed");

    const auto* ln2 = modelLoad.getNode(2);
    TEST_CHECK(ln2 != nullptr, "Node 2 loaded");
    TEST_CHECK(ln2->support().isPinned(), "Node 2 support is pinned");

    const auto* ln3 = modelLoad.getNode(3);
    TEST_CHECK(ln3 != nullptr, "Node 3 loaded");
    TEST_CHECK(ln3->support().isRoller(), "Node 3 support is roller");

    const auto* ln4 = modelLoad.getNode(4);
    TEST_CHECK(ln4 != nullptr, "Node 4 loaded");
    TEST_CHECK(ln4->support().hasSprings(), "Node 4 support has springs");
    TEST_CHECK(approxEqual(ln4->support().kx(), 8500.0), "Node 4 kx");
    TEST_CHECK(approxEqual(ln4->support().ky(), 9500.0), "Node 4 ky");
    TEST_CHECK(approxEqual(ln4->support().kz(), 25000.0), "Node 4 kz");
    TEST_CHECK(approxEqual(ln4->support().krx(), 150.0), "Node 4 krx");
    TEST_CHECK(approxEqual(ln4->support().kry(), 250.0), "Node 4 kry");
    TEST_CHECK(approxEqual(ln4->support().krz(), 350.0), "Node 4 krz");

    QFile::remove(tempFile);
    std::cout << "PASSED" << std::endl;
    return true;
}

bool test_SupportUndoRedoSnapshot()
{
    std::cout << "[Test 74] Support Undo/Redo State Snapshot Preservation ... ";

    TSA::Model::Model model;
    model.addNodeWithId(1, 0.0, 0.0, 0.0, "L0", "N1");
    model.getNode(1)->setSupport(TSA::Model::SupportDefinition::free());

    // Snapshot 1: Free
    model.pushUndoState("State 1: Free");

    // Modify to Fixed
    auto* nMut = model.getNode(1);
    nMut->setSupport(TSA::Model::SupportDefinition::fixed());
    model.notifyNodeModified(1);

    // Snapshot 2: Fixed
    model.pushUndoState("State 2: Fixed");

    // Modify to Elastic
    nMut = model.getNode(1);
    nMut->setSupport(TSA::Model::SupportDefinition::elastic(1234.0, 5678.0, 9999.0));
    model.notifyNodeModified(1);

    TEST_CHECK(model.getNode(1)->support().hasSprings(), "Currently elastic");

    // Undo -> Fixed
    model.undo();
    TEST_CHECK(model.getNode(1)->support().isFixed(), "After 1st undo, support should be fixed");

    // Undo -> Free
    model.undo();
    TEST_CHECK(model.getNode(1)->support().isFree(), "After 2nd undo, support should be free");

    // Redo -> Fixed
    model.redo();
    TEST_CHECK(model.getNode(1)->support().isFixed(), "After redo, support should be fixed again");

    std::cout << "PASSED" << std::endl;
    return true;
}

bool test_SupportCalculationSnapshot()
{
    std::cout << "[Test 75] Support Calculation Snapshot & OpenSees Boundary ... ";

    TSA::Model::Model model;
    model.addNodeWithId(1, 0.0, 0.0, 0.0);
    model.getNode(1)->setSupport(TSA::Model::SupportDefinition::fixed());

    model.addNodeWithId(2, 4.0, 0.0, 0.0);
    model.getNode(2)->setSupport(TSA::Model::SupportDefinition::pinned());

    model.addNodeWithId(3, 8.0, 0.0, 0.0);
    model.getNode(3)->setSupport(TSA::Model::SupportDefinition::elastic(500.0, 0.0, 1500.0));

    auto snap = TSA::Analysis::CalculationSnapshot::capture(model);

    // Node 1: Fixed
    const auto& sn1 = snap.nodes().at(1);
    TEST_CHECK(sn1.fixTx && sn1.fixTy && sn1.fixTz && sn1.fixRx && sn1.fixRy && sn1.fixRz, "Node 1 all DOFs fixed");

    // Node 2: Pinned
    const auto& sn2 = snap.nodes().at(2);
    TEST_CHECK(sn2.fixTx && sn2.fixTy && sn2.fixTz, "Node 2 translations fixed");
    TEST_CHECK(!sn2.fixRy && !sn2.fixRz, "Node 2 rotations free");

    // Node 3: Elastic
    const auto& sn3 = snap.nodes().at(3);
    TEST_CHECK(!sn3.fixTx, "Node 3 Tx not rigid");
    TEST_CHECK(approxEqual(sn3.kTx, 500.0), "Node 3 kTx stiffness");
    TEST_CHECK(approxEqual(sn3.kTz, 1500.0), "Node 3 kTz stiffness");

    // Boundary conditions Tcl generation
    std::string bcs = TSA::Analysis::OpenSeesAnalysisBuilder::buildBoundaryConditions(snap);
    TEST_CHECK(bcs.find("fix 1 1 1 1 1 1 1") != std::string::npos, "fix command for node 1 present");
    TEST_CHECK(bcs.find("fix 2 1 1 1") != std::string::npos, "fix command for node 2 present");

    std::cout << "PASSED" << std::endl;
    return true;
}

bool runSuite_Supports(int& passed)
{
    bool ok = true;
    if (test_SupportDefinitionPresets()) passed++; else ok = false;
    if (test_NodeSupportIntegration()) passed++; else ok = false;
    if (test_Support3DGeometry()) passed++; else ok = false;
    if (test_SupportFileSerialization()) passed++; else ok = false;
    if (test_SupportUndoRedoSnapshot()) passed++; else ok = false;
    if (test_SupportCalculationSnapshot()) passed++; else ok = false;
    return ok;
}
