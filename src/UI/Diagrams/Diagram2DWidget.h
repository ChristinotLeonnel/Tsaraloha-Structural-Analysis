#pragma once

#include "../../Analysis/AnalysisTypes.h"
#include <QWidget>
#include <QPainter>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>
#include <map>
#include <memory>
#include <vector>

#include "../../Analysis/ResultsModel.h"
#include "../../Geometry/DiagramGeometry.h"

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

/// Calculs du tracé des courbes 2D, indépendants du dessin (testés par la suite « display »).
namespace Diagram2D
{
/// Point d'une courbe : abscisse réduite s = x / L (0 à 1) et valeur de la grandeur.
struct Sample
{
    double s = 0.0;
    double v = 0.0;
};

struct Extremes
{
    double maxV = 0.0, maxS = 0.0; ///< plus grande valeur et sa position réduite
    double minV = 0.0, minS = 0.0; ///< plus petite valeur et sa position réduite
};

/// Points de la grandeur le long de la barre ; positions des stations ramenées à la longueur L (bornées à [0, 1]).
std::vector<Sample> sampleCurve(const TSA::Analysis::ElementResults& el, TSA::Geometry::DiagramType type, double length);
/// Valeur en s par interpolation linéaire entre stations ; s hors de la barre → valeur d'extrémité.
/// Deux stations à la même abscisse (saut sous une charge ponctuelle) : valeur de la première.
double interpolate(const std::vector<Sample>& samples, double s);
Extremes extremes(const std::vector<Sample>& samples);
/// Pas « rond » (1, 2 ou 5 × 10^n) donnant au plus environ maxTicks intervalles sur range.
double niceStep(double range, int maxTicks);
/// Texte d'une valeur avec un nombre de décimales adapté à son ordre de grandeur (rotations en rad lisibles).
QString formatValue(double v);
/// Grandeurs proposées par le widget, dans l'ordre d'affichage (efforts, déplacements, rotations).
const std::vector<TSA::Geometry::DiagramType>& availableTypes();
/// Libellé court (N (Vx), Vy, Mz, uz…) et couleur propre à chaque grandeur.
QString shortName(TSA::Geometry::DiagramType type);
QColor curveColor(TSA::Geometry::DiagramType type);
} // namespace Diagram2D

/**
 * @brief Widget interactif 2D des courbes de résultats le long d'une barre (N, V, M, T, déplacements, rotations).
 * Plusieurs grandeurs se superposent sur le même graphe (ex. N, Vy et Vz) ou s'empilent en bandes alignées sur la
 * même abscisse. Grandeurs d'unités différentes : un axe par unité (deux au plus, à gauche et à droite, zéros
 * alignés), sinon échelle normalisée. Le survol donne la valeur de chaque courbe ; extrêmes marqués à leur position.
 */
class Diagram2DWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Layout
    {
        Overlay, ///< courbes superposées sur un même graphe
        Stacked  ///< une bande par courbe, abscisses alignées
    };
    enum class Scaling
    {
        PerUnit,   ///< valeurs réelles, un axe par unité (normalisée au-delà de deux unités)
        Normalized ///< chaque courbe rapportée à sa plus grande valeur absolue (%)
    };

    explicit Diagram2DWidget(QWidget* parent = nullptr);
    ~Diagram2DWidget() override = default;

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);

    void setSelectedElement(int elementId, TSA::Analysis::StructuralElementKind kind = TSA::Analysis::StructuralElementKind::Beam);
    int selectedElement() const { return m_currentElementId; }
    TSA::Analysis::StructuralElementKind selectedElementKind() const { return m_currentElementKind; }

    /// Remplace les courbes affichées par une seule grandeur (commandes du ruban).
    void setDiagramType(TSA::Geometry::DiagramType type);
    /// Première courbe affichée (None si aucune).
    TSA::Geometry::DiagramType diagramType() const;
    /// Courbes affichées, dans l'ordre de availableTypes() ; les grandeurs inconnues du widget sont ignorées.
    void setDiagramTypes(const std::vector<TSA::Geometry::DiagramType>& types);
    std::vector<TSA::Geometry::DiagramType> diagramTypes() const;
    /// Ajoute ou retire une courbe (la dernière courbe ne peut pas être retirée).
    void setDiagramTypeVisible(TSA::Geometry::DiagramType type, bool visible);

    void setCurveLayout(Layout layout);
    Layout curveLayout() const { return m_layout; }
    void setScaling(Scaling scaling);
    Scaling scaling() const { return m_scaling; }
    /// Échelle réellement appliquée : normalisée si plus de deux unités sont superposées.
    Scaling effectiveScaling() const;

    /// Valeurs des courbes affichées (x en m puis une colonne par grandeur, séparateur tabulation), pour un tableur.
    QString valuesTable() const;
    /// Image du graphe seul (sans barre d'outils ni curseur).
    QImage renderImage(const QSize& size) const;

signals:
    void elementInspected(int elementId, double posNorm, double forceValue);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onElementComboChanged(int index);
    void onPresetComboChanged(int index);
    void onResetZoom();

private:
    struct Curve
    {
        TSA::Geometry::DiagramType type = TSA::Geometry::DiagramType::None;
        QString name;
        QString unit;
        QColor color;
        std::vector<Diagram2D::Sample> samples;
        Diagram2D::Extremes ext;
        int axis = 0; ///< groupe d'échelle (unité ou courbe)
    };
    /// Échelle verticale d'un groupe : valeur = coordonnée réduite × magnitude, coordonnée dans [low, high].
    struct AxisScale
    {
        QString unit;
        double magnitude = 1.0;
    };
    struct Frame
    {
        std::vector<Curve> curves;
        std::vector<AxisScale> axes;
        double low = -1.0, high = 1.0; ///< bornes réduites communes (zéro aligné entre axes)
        bool normalized = false;
        double length = 1.0;
        QString message; ///< texte affiché à la place du graphe
    };

    void setupUi();
    void updateElementList();
    int findElementItem(TSA::Analysis::StructuralElementKind kind, int id) const;
    const TSA::Analysis::ElementResults* currentResults() const;
    double currentLength(const TSA::Analysis::ElementResults* el) const;

    Frame buildFrame() const;
    static void computeRange(const Frame& f, const std::vector<int>& curveIndices, double zoom, double& low, double& high);
    QRect plotArea() const;
    QRect plotRectIn(const QRect& area, const Frame& f) const;
    std::vector<QRect> stripRects(const QRect& plotRect, int count) const;
    void paintPlot(QPainter& p, const QRect& area, bool withCursor) const;
    void drawOverlay(QPainter& p, const QRect& plotRect, const Frame& f, bool withCursor) const;
    void drawStacked(QPainter& p, const QRect& plotRect, const Frame& f, bool withCursor) const;
    void drawXAxis(QPainter& p, const QRect& plotRect, double length, bool labels) const;
    void drawCurve(QPainter& p, const QRect& r, const Curve& c, double low, double high, double magnitude, int fillAlpha) const;
    void drawExtremes(QPainter& p, const QRect& r, const Curve& c, double low, double high, double magnitude,
                      std::vector<QRect>& placed) const;
    QRect drawLegend(QPainter& p, const QRect& plotRect, const Frame& f) const;
    void drawCursor(QPainter& p, const QRect& plotRect, const Frame& f, const std::vector<QRect>& strips) const;
    double sToX(const QRect& r, double s) const;
    double xToS(const QRect& r, double x) const;

    void syncChips();
    void syncPresetCombo();
    void saveSettings() const;
    void restoreSettings();
    void copyImage();
    void saveImage();
    void copyValues();

private:
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;

    std::vector<TSA::Geometry::DiagramType> m_types{TSA::Geometry::DiagramType::BendingMz};
    Layout m_layout = Layout::Overlay;
    Scaling m_scaling = Scaling::PerUnit;
    int m_currentElementId = 0;
    TSA::Analysis::StructuralElementKind m_currentElementKind = TSA::Analysis::StructuralElementKind::Beam; ///< ids poutre/poteau non uniques entre familles

    // Commandes UI
    QWidget* m_toolbar = nullptr;
    QComboBox* m_elementCombo = nullptr;
    QComboBox* m_presetCombo = nullptr;
    QComboBox* m_layoutCombo = nullptr;
    QComboBox* m_scaleCombo = nullptr;
    QPushButton* m_btnResetZoom = nullptr;
    QToolButton* m_btnExport = nullptr;
    QLabel* m_lblStatus = nullptr;
    std::map<TSA::Geometry::DiagramType, QToolButton*> m_chips;

    // Curseur interactif
    bool m_hasHoverCursor = false;
    QPoint m_hoverPixel;
    double m_hoverS = 0.0;

    // Zoom vertical (molette), fenêtre horizontale [m_viewS0, m_viewS1] (Ctrl + molette, glisser pour déplacer)
    double m_zoomFactor = 1.0;
    double m_viewS0 = 0.0;
    double m_viewS1 = 1.0;
    bool m_panning = false;
    int m_panLastX = 0;
};

} // namespace TSA::UI
