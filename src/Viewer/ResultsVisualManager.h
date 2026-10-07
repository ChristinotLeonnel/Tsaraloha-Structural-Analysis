#pragma once

#include <QObject>
#include <memory>
#include <map>
#include <vector>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>

#include "../Analysis/ResultsModel.h"
#include "../Geometry/DiagramGeometry.h"
#include "../Geometry/DeformedGeometry.h"

namespace TSA::Model
{
class Model;
}

class OccView;

namespace TSA::NDC
{
struct ExtremumPoint;
}

namespace TSA::Viewer
{

enum class DeformedDisplayMode
{
    UndeformedOnly,
    DeformedOnly,
    Both
};

enum class ScalePreset
{
    Auto,
    X1,
    X10,
    X100,
    X1000,
    X10000,
    Custom
};

/**
 * @brief Gestionnaire de visualisation 3D des résultats de calcul OpenSees dans le viewport OCCT.
 * Responsable du rendu de la déformée (amplification, superposition), des diagrammes 3D (N, V, M),
 * et des flèches de réaction aux appuis.
 */
class ResultsVisualManager : public QObject
{
    Q_OBJECT

public:
    explicit ResultsVisualManager(OccView* occView, QObject* parent = nullptr);
    ~ResultsVisualManager() override;

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);
    std::shared_ptr<TSA::Analysis::ResultsModel> resultsModel() const { return m_results; }

    bool hasResults() const;

    // Déformée
    bool isDeformedVisible() const { return m_deformedVisible; }
    void setDeformedVisible(bool visible);

    DeformedDisplayMode deformedDisplayMode() const { return m_displayMode; }
    void setDeformedDisplayMode(DeformedDisplayMode mode);

    double deformationScale() const { return m_deformationScale; }
    void setDeformationScale(double scale);
    void autoComputeDeformationScale();

    ScalePreset deformationScalePreset() const { return m_deformationPreset; }
    void setDeformationScalePreset(ScalePreset preset, double customVal = 1.0);

    // Diagrammes 3D
    TSA::Geometry::DiagramType diagramType() const { return m_diagramType; }
    void setDiagramType(TSA::Geometry::DiagramType type);

    double diagramScale() const { return m_diagramScale; }
    void setDiagramScale(double scale);
    void autoComputeDiagramScale();

    ScalePreset diagramScalePreset() const { return m_diagramPreset; }
    void setDiagramScalePreset(ScalePreset preset, double customVal = 0.05);

    bool areDiagramLabelsVisible() const { return m_diagramLabelsVisible; }
    void setDiagramLabelsVisible(bool visible);

    // Réactions aux appuis
    bool areReactionsVisible() const { return m_reactionsVisible; }
    void setReactionsVisible(bool visible);

    // Étape active (Incrément non-linéaire)
    int activeStep() const;
    void setActiveStep(int step);

    // Légende
    bool isLegendVisible() const { return m_legendVisible; }
    void setLegendVisible(bool visible);
    QString legendSummaryText() const;

    // Marqueur 3D d'extremum (Note de Calcul)
    void showExtremumMarker(const TSA::NDC::ExtremumPoint& pt);
    void clearExtremumMarker();

    // Nettoyage et actualisation
    void updateAllVisuals();
    void clearAllVisuals();

signals:
    void visualStateChanged();

private:
    void updateDeformedShapes();
    void clearDeformedShapes();

    void updateDiagramShapes();
    void clearDiagramShapes();

    void updateReactionShapes();
    void clearReactionShapes();

    Handle(AIS_InteractiveContext) context() const;
    void redrawView();

private:
    OccView* m_occView = nullptr;
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;

    bool m_deformedVisible = true;
    DeformedDisplayMode m_displayMode = DeformedDisplayMode::DeformedOnly;
    double m_deformationScale = 50.0;

    TSA::Geometry::DiagramType m_diagramType = TSA::Geometry::DiagramType::None;
    double m_diagramScale = 0.05;
    bool m_diagramLabelsVisible = true;

    bool m_reactionsVisible = true;

    ScalePreset m_deformationPreset = ScalePreset::Auto;
    ScalePreset m_diagramPreset = ScalePreset::Auto;
    bool m_legendVisible = true;

    // Objets OCCT affichés
    std::map<int, Handle(AIS_Shape)> m_deformedElementShapes;
    std::map<int, Handle(AIS_Shape)> m_deformedNodeShapes;
    std::map<int, Handle(AIS_Shape)> m_diagramShapes;
    std::map<int, std::vector<Handle(AIS_TextLabel)>> m_diagramLabels;
    std::map<int, Handle(AIS_Shape)> m_reactionShapes;
    std::map<int, Handle(AIS_TextLabel)> m_reactionLabels;
    Handle(AIS_Shape) m_extremumMarkerShape;
    Handle(AIS_TextLabel) m_extremumLabel;
    Handle(AIS_TextLabel) m_legendLabel;

    void updateLegend();
    void clearLegend();
};

} // namespace TSA::Viewer
