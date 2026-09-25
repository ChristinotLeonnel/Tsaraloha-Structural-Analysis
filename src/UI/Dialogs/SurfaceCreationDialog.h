#pragma once

#include <QDialog>
#include <gp_Pnt.hxx>
#include <vector>
#include <QString>
#include <QColor>

class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QToolButton;
class QLabel;
class QStackedWidget;

class OccView;

namespace TSA::Model
{
    class Model;
}

namespace TSA::UI
{

/**
 * @brief Dialogue unifié pour la modélisation et le dessin d'éléments surfaciques (Dalles, Voiles/Murs).
 *
 * Regroupe tous les éléments surfaciques 2D dans une seule interface interactive,
 * permettant à la fois la saisie numérique (numéros de nœuds ou coordonnées) et le tracé direct
 * interactif avec prévisualisation en temps réel dans le viewport 3D (style Robot Structural Analysis).
 */
class SurfaceCreationDialog : public QDialog
{
    Q_OBJECT

public:
    enum class SurfaceType
    {
        Slab = 0,
        Wall = 1
    };

    explicit SurfaceCreationDialog(TSA::Model::Model* model, OccView* occView, QWidget* parent = nullptr);
    ~SurfaceCreationDialog() override = default;

    SurfaceType currentSurfaceType() const;
    void setSurfaceType(SurfaceType type);

    void setSlabThickness(double th);
    void setWallParameters(double th, double height, double offset);

public slots:
    void onSlabNodePicked(int nodeId, const gp_Pnt& pt, int totalCount);
    void onSlabCreated(int slabId);
    void onSlabDrawingCancelled();

    void onWallFirstPointPicked(const gp_Pnt& pt, int nodeId);
    void onWallSecondPointPicked(const gp_Pnt& pt, int nodeId);
    void onWallDrawingCancelled();
    void onWallCreated(int wallId);

signals:
    void surfaceTypeChanged(SurfaceType type);
    void surfaceCreated(int id, SurfaceType type);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onTypeChanged(int index);
    void onAddClicked();
    void onCloseContourClicked();
    void onClearContourClicked();
    void onHelpClicked();
    void onColorPickClicked();
    void onWallOriginReturnPressed();
    void onWallEndReturnPressed();

private:
    void setupUi();
    void updateNextId();
    void syncPresetsToOccView();
    bool parseCoordinates(const QString& text, double& x, double& y, double& z) const;
    int resolveOrCreateNode(const QString& text);
    QString formatPoint(const gp_Pnt& pt) const;
    void updateColorButton(QPushButton* btn, const QColor& col);

private:
    TSA::Model::Model* m_model = nullptr;
    OccView* m_occView = nullptr;

    // Type selector
    QComboBox* m_comboType = nullptr;

    // Numbering & Naming
    QSpinBox* m_spinId = nullptr;
    QSpinBox* m_spinStep = nullptr;
    QLineEdit* m_editName = nullptr;

    // Stacked widget for Slab vs Wall parameters
    QStackedWidget* m_paramStack = nullptr;

    // --- Slab widgets ---
    QWidget* m_slabPage = nullptr;
    QDoubleSpinBox* m_spinSlabThickness = nullptr;
    QComboBox* m_comboSlabType = nullptr; // Bidirectionnelle / Unidirectionnelle
    QComboBox* m_comboSlabMaterial = nullptr;
    QPushButton* m_btnSlabColor = nullptr;
    QLineEdit* m_editSlabNodes = nullptr; // Contour node IDs
    QLabel* m_lblSlabStatus = nullptr;
    QPushButton* m_btnCloseContour = nullptr;
    QPushButton* m_btnClearContour = nullptr;
    QColor m_slabColor;
    std::vector<int> m_currentSlabNodes;

    // --- Wall widgets ---
    QWidget* m_wallPage = nullptr;
    QDoubleSpinBox* m_spinWallThickness = nullptr;
    QDoubleSpinBox* m_spinWallHeight = nullptr;
    QDoubleSpinBox* m_spinWallOffset = nullptr;
    QComboBox* m_comboWallMaterial = nullptr;
    QPushButton* m_btnWallColor = nullptr;
    QLineEdit* m_editWallOrigin = nullptr;
    QLineEdit* m_editWallEnd = nullptr;
    QCheckBox* m_chkWallChain = nullptr;
    QColor m_wallColor;

    // Action buttons
    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnClose = nullptr;
    QPushButton* m_btnHelp = nullptr;
};

} // namespace TSA::UI
