#pragma once

#include <QDialog>
#include "../../Model/Beam.h"
#include "../../Model/Section.h"
#include "../../Model/Material.h"
#include <gp_Pnt.hxx>

class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QToolButton;

class OccView;

namespace TSA::Model
{
    class Model;
}

namespace TSA::UI
{

class BarCreationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BarCreationDialog(TSA::Model::Model* model, OccView* occView, QWidget* parent = nullptr);
    ~BarCreationDialog() override = default;

    TSA::Model::BarProperties currentProperties() const;
    void setRole(TSA::Model::BarRole role);
    void setSection(const TSA::Model::Section& section);
    void setRotation(double gammaDegrees);
    bool isChainMode() const;

    void loadFromBar(const TSA::Model::Bar& bar);

public slots:
    void onFirstPointPicked(const gp_Pnt& pt, int nodeId);
    void onSecondPointPicked(const gp_Pnt& pt, int nodeId);
    void onDrawingCancelled();

signals:
    void barPropertiesChanged(const TSA::Model::BarProperties& props);
    void manualPointEntered(const gp_Pnt& pt, bool isFirstPoint);
    void barCreated(int barId);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onRoleChanged(int index);
    void onSectionChanged(int index);
    void onRotationChanged(double val);
    void onEccentricityChanged(int index);
    void onOriginReturnPressed();
    void onEndReturnPressed();
    void onAddClicked();
    void onCustomSectionRequested();
    void onHelpClicked();

private:
    void setupUi();
    void populateSections();
    void updateHighlight(bool waitingForSecondPoint);
    bool parseCoordinates(const QString& text, double& x, double& y, double& z) const;
    QString formatPoint(const gp_Pnt& pt) const;
    void emitPropertiesChanged();

private:
    TSA::Model::Model* m_model = nullptr;
    OccView* m_occView = nullptr;

    // Numérotation
    QSpinBox* m_spinBarId = nullptr;
    QSpinBox* m_spinStep = nullptr;
    QLineEdit* m_editName = nullptr;
    QToolButton* m_btnNameMore = nullptr;

    // Caractéristiques
    QComboBox* m_comboRole = nullptr;
    QToolButton* m_btnRoleMore = nullptr;
    QComboBox* m_comboSection = nullptr;
    QToolButton* m_btnSectionMore = nullptr;
    QLineEdit* m_editMaterial = nullptr;

    // Coordonnées des nœuds
    QLineEdit* m_editOrigin = nullptr;
    QLineEdit* m_editEnd = nullptr;
    QCheckBox* m_chkChain = nullptr; // "Étirer"

    // Position de l'axe
    QComboBox* m_comboEccentricity = nullptr;
    QToolButton* m_btnEccMore = nullptr;
    QDoubleSpinBox* m_spinRotation = nullptr;

    QPushButton* m_btnCustomizeSection = nullptr;
    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnClose = nullptr;
    QPushButton* m_btnHelp = nullptr;

    // État interne
    gp_Pnt m_originPt;
    gp_Pnt m_endPt;
    int m_originNodeId = -1;
    int m_endNodeId = -1;
    bool m_hasOrigin = false;
    std::vector<TSA::Model::Section> m_sectionLibrary;
};

} // namespace TSA::UI
