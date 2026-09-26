#pragma once

#include "Cable.h"
#include <vector>
#include <string>

namespace TSA::Model
{

class Model;

/**
 * @brief Modèle complet d'un système de pont suspendu (Suspension Bridge System).
 * Comprend le câble porteur principal (caténaire), les suspentes verticales,
 * les liaisons aux pylônes et massifs d'ancrage.
 */
class SuspensionBridge
{
public:
    SuspensionBridge();
    explicit SuspensionBridge(const std::string& name);

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Identifiants des nœuds clés de suspension
    int leftAnchorNodeId() const { return m_leftAnchorNodeId; }
    void setLeftAnchorNodeId(int id) { m_leftAnchorNodeId = id; }

    int leftTowerNodeId() const { return m_leftTowerNodeId; }
    void setLeftTowerNodeId(int id) { m_leftTowerNodeId = id; }

    int rightTowerNodeId() const { return m_rightTowerNodeId; }
    void setRightTowerNodeId(int id) { m_rightTowerNodeId = id; }

    int rightAnchorNodeId() const { return m_rightAnchorNodeId; }
    void setRightAnchorNodeId(int id) { m_rightAnchorNodeId = id; }

    // Nœuds du tablier à suspendre
    const std::vector<int>& deckNodeIds() const { return m_deckNodeIds; }
    void setDeckNodeIds(const std::vector<int>& ids) { m_deckNodeIds = ids; }
    void addDeckNodeId(int id) { m_deckNodeIds.push_back(id); }

    // Câble porteur principal
    int mainCableId() const { return m_mainCableId; }
    void setMainCableId(int id) { m_mainCableId = id; }

    double mainSpan() const { return m_mainSpan; }
    void setMainSpan(double span) { m_mainSpan = std::max(1.0, span); }

    double mainSpanSag() const { return m_mainSpanSag; }
    void setMainSpanSag(double sag) { m_mainSpanSag = std::max(0.1, sag); }
    double sag() const { return m_mainSpanSag; }
    void setSag(double s) { setMainSpanSag(s); }

    const CableDefinition& mainCableDefinition() const { return m_mainCableDef; }
    void setMainCableDefinition(const CableDefinition& def) { m_mainCableDef = def; }

    // Suspentes (Hangers)
    const std::vector<int>& hangerCableIds() const { return m_hangerCableIds; }
    void setHangerCableIds(const std::vector<int>& ids) { m_hangerCableIds = ids; }

    const CableDefinition& hangerDefinition() const { return m_hangerDef; }
    void setHangerDefinition(const CableDefinition& def) { m_hangerDef = def; }

    double hangerSpacing() const { return m_hangerSpacing; }
    void setHangerSpacing(double spacing) { m_hangerSpacing = std::max(0.5, spacing); }

    /**
     * @brief Génère automatiquement dans le Model TSA :
     * 1. Le câble porteur principal (parabolique ou caténaire) entre les deux sommets de pylônes
     * 2. Les câbles de retenue (backstays) vers les massifs d'ancrage
     * 3. L'ensemble des suspentes verticales descendant du câble principal vers chaque nœud du tablier
     * 4. Les nœuds de fixation hauts sur le câble principal
     *
     * @param model Référence vers le modèle structural TSA
     * @return true si la génération est un succès complet
     */
    bool generateInModel(Model& model);

    /**
     * @brief Génère des suspentes reliant un câble porteur existant à une liste de nœuds de tablier.
     */
    std::vector<int> generateHangers(Model& model, int mainCableId, const std::vector<int>& deckNodeIds);
    static std::vector<int> generateHangers(Model& model, int mainCableId, const std::vector<int>& deckNodeIds, const CableDefinition& hangerDef);

private:
    std::string m_name = "SuspensionBridge_1";
    int m_leftAnchorNodeId = -1;
    int m_leftTowerNodeId = -1;
    int m_rightTowerNodeId = -1;
    int m_rightAnchorNodeId = -1;

    std::vector<int> m_deckNodeIds;
    int m_mainCableId = -1;
    std::vector<int> m_hangerCableIds;

    double m_mainSpan = 100.0;       // Portée principale (m)
    double m_mainSpanSag = 15.0;     // 15 m de flèche au centre de la travée principale
    double m_hangerSpacing = 10.0;   // 10 m d'espacement des suspentes

    CableDefinition m_mainCableDef;
    CableDefinition m_hangerDef;
};

} // namespace TSA::Model
