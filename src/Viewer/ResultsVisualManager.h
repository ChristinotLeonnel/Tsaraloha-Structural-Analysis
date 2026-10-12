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
    /// Animation de la déformée : fraction (0…1) du facteur d'amplification appliquée à l'affichage. Les mêmes
    /// résultats statiques sont réaffichés (aucun calcul relancé, aucune analyse dynamique).
    void setAnimationPhase(double phase);
    double animationPhase() const { return m_animationPhase; }
    void setDeformationScalePreset(ScalePreset preset, double customVal = 1.0);

    /// Rotation des sections (torsion θx des poutres et poteaux) appliquée à la déformée, avec repères de
    /// section. Activée par défaut ; désactivée, la déformée ne montre que les déplacements de l'axe.
    bool isSectionRotationVisible() const { return m_sectionRotationVisible; }
    void setSectionRotationVisible(bool visible);
    /// Plus grande torsion |θx| calculée (rad, non amplifiée) et barre concernée.
    double maxTwist() const;
    std::string maxTwistElement() const;
    /// Nombre de barres dont les repères de torsion sont affichés (0 sans vue 3D).
    std::size_t sectionMarkerCount() const { return m_twistMarkerShapes.size(); }

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

    /// Objets de résultats actuellement affichés (diagnostic : un objet de la scène absent de ces comptes serait un
    /// objet orphelin, impossible à retirer).
    struct Diagnostics
    {
        int deformed = 0, twistMarkers = 0, diagrams = 0, diagramLabels = 0, reactions = 0, reactionLabels = 0, extremum = 0, legend = 0;
        int total() const { return deformed + twistMarkers + diagrams + diagramLabels + reactions + reactionLabels + extremum + legend; }
    };
    Diagnostics diagnostics() const;

    // Nettoyage et actualisation
    void updateAllVisuals();
    void clearAllVisuals();

signals:
    void visualStateChanged();

private:
    void updateDeformedShapes();
    void clearDeformedShapes();
    /// Axes déformés NON amplifiés, calculés une fois par résultat (un changement d'échelle ne
    /// reconstruit que la géométrie d'affichage).
    void rebuildDeformedAxes();
    void applyStructureDisplay(bool deformedShown);
    const TSA::Model::Section* memberSection(const TSA::Analysis::ElementKey& key, double* rotationDeg) const;

    void updateDiagramShapes();
    void clearDiagramShapes();

    void updateReactionShapes();
    void clearReactionShapes();

    Handle(AIS_InteractiveContext) context() const;
    template <typename Map, typename Key>
    void replaceShape(Map& map, const Key& key, const Handle(AIS_Shape)& shape);
    void redrawView();

private:
    OccView* m_occView = nullptr;
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;

    bool m_deformedVisible = true;
    // Déformée et structure initiale atténuée : les deux restent lisibles et distinctes.
    DeformedDisplayMode m_displayMode = DeformedDisplayMode::Both;
    double m_deformationScale = 50.0;

    TSA::Geometry::DiagramType m_diagramType = TSA::Geometry::DiagramType::None;
    double m_diagramScale = 0.05;
    bool m_diagramLabelsVisible = true;

    bool m_reactionsVisible = true;

    ScalePreset m_deformationPreset = ScalePreset::Auto;
    double m_animationPhase = 1.0;
    ScalePreset m_diagramPreset = ScalePreset::Auto;
    bool m_legendVisible = true;

    // Axes déformés (données de visualisation, jamais réinjectées dans le modèle)
    std::map<TSA::Analysis::ElementKey, TSA::Geometry::DeformedAxis> m_deformedAxes;
    bool m_deformedAxesValid = false;
    bool m_shuttingDown = false;
    int m_axesFromStations = 0;
    int m_axesFromHermite = 0;
    int m_axesInvalid = 0;
    std::string m_firstAxisProblem;
    bool m_sectionRotationVisible = true;
    double m_maxTwist = 0.0;           ///< |θx| max (rad), calculé avec les axes
    std::string m_maxTwistElement;

    // Objets OCCT affichés
    // Objets affichés par élément, désignés par (famille, id) : les ids TSA ne sont uniques que dans leur famille
    // (poutre 1 et poteau 1 coexistent). Une clé « int » faisait écraser un objet affiché sans le retirer de la scène :
    // il restait à l'écran indéfiniment, y compris dans un nouveau projet (audit de l'affichage).
    std::map<TSA::Analysis::ElementKey, Handle(AIS_Shape)> m_deformedElementShapes;
    std::map<int, Handle(AIS_Shape)> m_deformedNodeShapes;
    std::vector<Handle(AIS_Shape)> m_twistMarkerShapes;
    std::map<TSA::Analysis::ElementKey, Handle(AIS_Shape)> m_diagramShapes;
    std::map<TSA::Analysis::ElementKey, std::vector<Handle(AIS_TextLabel)>> m_diagramLabels;
    std::map<int, Handle(AIS_Shape)> m_reactionShapes;
    std::map<int, Handle(AIS_TextLabel)> m_reactionLabels;
    Handle(AIS_Shape) m_extremumMarkerShape;
    Handle(AIS_TextLabel) m_extremumLabel;
    Handle(AIS_TextLabel) m_legendLabel;

    void updateLegend();
    void clearLegend();
};

} // namespace TSA::Viewer
