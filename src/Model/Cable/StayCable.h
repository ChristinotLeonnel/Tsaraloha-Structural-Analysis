#pragma once

#include "Cable.h"

namespace TSA::Model
{

/**
 * @brief Configuration d'éventail de haubanage (Stay Cable System).
 */
enum class StaySystemType
{
    Fan,        ///< Système en éventail convergent au sommet du pylône
    Harp,       ///< Système en harpe (haubans parallèles régulièrement espacés)
    SemiFan,    ///< Système en semi-éventail (le plus courant en grands ponts haubanés)
    Star        ///< Système en étoile
};

using StaySystemMode = StaySystemType;

/**
 * @brief Spécialisation d'un hauban pour ponts et structures haubanées.
 */
class StayCable : public Cable
{
public:
    StayCable();
    StayCable(int id, int towerNodeId, int deckNodeId, const std::string& name, const CableDefinition& definition = CableDefinition::stayCablePSS_19x15_7());
    StayCable(int id, int towerNodeId, int deckNodeId, const CableDefinition& definition, const std::string& name = "");

    int towerNodeId() const { return startNodeId(); }
    void setTowerNodeId(int nodeId) { setStartNodeId(nodeId); }

    int deckNodeId() const { return endNodeId(); }
    void setDeckNodeId(int nodeId) { setEndNodeId(nodeId); }

    int stayIndex() const { return m_stayIndex; }
    void setStayIndex(int idx) { m_stayIndex = idx; }

    int planeIndex() const { return m_planeIndex; }
    void setPlaneIndex(int plane) { m_planeIndex = plane; } // 0 = nappe axiale ou gauche, 1 = nappe droite

    StaySystemType systemType() const { return m_systemType; }
    void setSystemType(StaySystemType type) { m_systemType = type; }
    void setStaySystem(StaySystemType type) { setSystemType(type); }

    bool hasDamper() const { return m_hasDamper; }
    void setHasDamper(bool has) { m_hasDamper = has; }

    double damperPositionFromDeck() const { return m_damperPos; }
    void setDamperPositionFromDeck(double posMeters) { m_damperPos = posMeters; }

    // Angle d'inclinaison par rapport au tablier horizontal en degrés
    double inclinationAngleDeg(const Model& model) const;
    double inclinationDegrees(const Model& model) const { return inclinationAngleDeg(model); }

private:
    int m_stayIndex = 1;
    int m_planeIndex = 0;
    StaySystemType m_systemType = StaySystemType::SemiFan;
    bool m_hasDamper = false;
    double m_damperPos = 2.5; // amortisseur à 2.5 m du tablier
};

} // namespace TSA::Model
