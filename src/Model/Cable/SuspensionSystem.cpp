#include "SuspensionSystem.h"
#include "../Model.h"
#include "../Node.h"
#include <cmath>
#include <algorithm>

namespace TSA::Model
{

SuspensionBridge::SuspensionBridge()
    : m_name("SuspensionBridge_1")
    , m_mainCableDef(CableDefinition::suspensionCableFLC_120())
    , m_hangerDef(CableDefinition::hangerCable_30())
{
}

SuspensionBridge::SuspensionBridge(const std::string& name)
    : m_name(name)
    , m_mainCableDef(CableDefinition::suspensionCableFLC_120())
    , m_hangerDef(CableDefinition::hangerCable_30())
{
}

std::vector<int> SuspensionBridge::generateHangers(Model& model, int mainCableId, const std::vector<int>& deckNodeIds)
{
    return generateHangers(model, mainCableId, deckNodeIds, m_hangerDef);
}

std::vector<int> SuspensionBridge::generateHangers(
    Model& model, 
    int mainCableId, 
    const std::vector<int>& deckNodeIds, 
    const CableDefinition& hangerDef)
{
    std::vector<int> createdHangerIds;
    auto* mainCable = model.getCable(mainCableId);
    if (!mainCable)
    {
        return createdHangerIds;
    }

    const auto* nTowerA = model.getNode(mainCable->startNodeId());
    const auto* nTowerB = model.getNode(mainCable->endNodeId());
    if (!nTowerA || !nTowerB)
    {
        return createdHangerIds;
    }

    gp_Pnt pA(nTowerA->x(), nTowerA->y(), nTowerA->z());
    gp_Pnt pB(nTowerB->x(), nTowerB->y(), nTowerB->z());

    double dxAB = pB.X() - pA.X();
    double dyAB = pB.Y() - pA.Y();
    double chordLen2D = std::sqrt(dxAB * dxAB + dyAB * dyAB);
    if (chordLen2D < 1e-4)
    {
        return createdHangerIds;
    }

    CableGeometry geom = mainCable->geometry();
    geom.setStartPoint(pA);
    geom.setEndPoint(pB);

    int hangerIndex = 1;
    for (int deckNodeId : deckNodeIds)
    {
        const auto* nDeck = model.getNode(deckNodeId);
        if (!nDeck)
            continue;

        // Projection du nœud de tablier sur l'axe horizontal du câble
        double dx = nDeck->x() - pA.X();
        double dy = nDeck->y() - pA.Y();
        double projDist = (dx * dxAB + dy * dyAB) / chordLen2D;
        double t = projDist / chordLen2D;

        // Seuls les nœuds situés entre les pylônes sont suspendus
        if (t <= 0.01 || t >= 0.99)
        {
            continue;
        }

        // Calcul du point d'accroche 3D sur le câble porteur principal
        gp_Pnt pCable = geom.evaluatePoint(t);

        // Création du nœud supérieur d'attache sur le câble
        std::string topNodeName = "NH_Top_" + std::to_string(hangerIndex);
        int topNodeId = model.addNode(pCable.X(), pCable.Y(), pCable.Z(), "", topNodeName);

        // Création de la suspente verticale (câble droit)
        std::string hangerName = "Hanger_" + std::to_string(hangerIndex);
        int hId = model.addCable(topNodeId, deckNodeId, hangerDef, hangerName, CableGeometryMode::Straight);
        if (hId > 0)
        {
            auto* cablePtr = model.getCable(hId);
            if (cablePtr)
            {
                cablePtr->setType(CableType::Hanger);
                cablePtr->setColor("#3399FF"); // Couleur bleue distinctive pour suspentes
            }
            createdHangerIds.push_back(hId);
            hangerIndex++;
        }
    }

    return createdHangerIds;
}

bool SuspensionBridge::generateInModel(Model& model)
{
    if (m_leftTowerNodeId <= 0 || m_rightTowerNodeId <= 0)
    {
        return false;
    }

    // 1. Câble porteur principal entre pylônes
    std::string mcName = m_name + "_MainCable";
    m_mainCableId = model.addCable(
        m_leftTowerNodeId, 
        m_rightTowerNodeId, 
        m_mainCableDef, 
        mcName, 
        CableGeometryMode::Parabolic, 
        m_mainSpanSag
    );

    if (m_mainCableId <= 0)
    {
        return false;
    }

    auto* mc = model.getCable(m_mainCableId);
    if (mc)
    {
        mc->setType(CableType::SuspensionCable);
        mc->setColor("#FF8C00"); // Teinte ambrée pour câble principal
    }

    // 2. Retenues extérieures (Backstays vers les massifs d'ancrage)
    if (m_leftAnchorNodeId > 0)
    {
        std::string bsLeftName = m_name + "_Backstay_Left";
        model.addCable(m_leftAnchorNodeId, m_leftTowerNodeId, m_mainCableDef, bsLeftName, CableGeometryMode::Straight);
    }
    if (m_rightAnchorNodeId > 0)
    {
        std::string bsRightName = m_name + "_Backstay_Right";
        model.addCable(m_rightTowerNodeId, m_rightAnchorNodeId, m_mainCableDef, bsRightName, CableGeometryMode::Straight);
    }

    // 3. Génération des suspentes vers le tablier
    if (!m_deckNodeIds.empty())
    {
        m_hangerCableIds = generateHangers(model, m_mainCableId, m_deckNodeIds, m_hangerDef);
    }

    return true;
}

} // namespace TSA::Model
