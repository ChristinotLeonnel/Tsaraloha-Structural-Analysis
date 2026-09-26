#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <gp_Pnt.hxx>
#include "../Model/CreationPresets.h"
#include "../Model/Beam.h"

namespace TSA::Interaction
{

enum class InteractionMode
{
    Select,
    DrawNode,
    DrawBar,
    DrawBeam,
    DrawColumn,
    DrawSlab,
    DrawWall,
    DrawFoundation,
    DrawTruss,
    Move3D,
    Copy3D,
    Rotate3D,
    MoveOrigin3D,
    Paste3D
};

/**
 * @brief Gestionnaire centralisé des modes d'interaction 3D et des états de capture d'outils.
 */
class InteractionManager : public QObject
{
    Q_OBJECT

public:
    explicit InteractionManager(QObject* parent = nullptr);
    ~InteractionManager() override = default;

    InteractionMode mode() const noexcept { return m_mode; }
    void setMode(InteractionMode mode);

    bool isDrawingMode() const noexcept;
    bool isTransformMode() const noexcept;

    // Préréglages et propriétés de création
    const TSA::Model::StructurePresets& presets() const noexcept { return m_presets; }
    TSA::Model::StructurePresets& presets() noexcept { return m_presets; }
    void setPresets(const TSA::Model::StructurePresets& p) { m_presets = p; }

    const TSA::Model::BarProperties& currentBarProperties() const noexcept { return m_currentBarProps; }
    void setCurrentBarProperties(const TSA::Model::BarProperties& props) { m_currentBarProps = props; }

    // État de capture du premier point
    bool hasStartPoint() const noexcept { return m_hasStartPoint; }
    const gp_Pnt& startPoint() const noexcept { return m_startPoint; }
    int startNodeId() const noexcept { return m_startNodeId; }

    void setStartPoint(const gp_Pnt& pt, int nodeId = 0);
    void resetDrawingState();

    // Saisie surfacique (Dalles / Voiles)
    const std::vector<gp_Pnt>& slabPoints() const noexcept { return m_slabPoints; }
    const std::vector<int>& slabNodeIds() const noexcept { return m_slabNodeIds; }
    void addSlabPoint(const gp_Pnt& pt, int nodeId = 0);

    QString promptText() const;

signals:
    void modeChanged(InteractionMode newMode);
    void promptChanged(const QString& prompt);
    void drawingStateReset();

private:
    InteractionMode m_mode = InteractionMode::Select;
    TSA::Model::StructurePresets m_presets;
    TSA::Model::BarProperties m_currentBarProps;

    bool m_hasStartPoint = false;
    gp_Pnt m_startPoint;
    int m_startNodeId = 0;

    std::vector<gp_Pnt> m_slabPoints;
    std::vector<int> m_slabNodeIds;
};

} // namespace TSA::Interaction
