#pragma once

// Dialogues du maillage (docs/MESH.md) :
//   LocalMeshDialog      paramètres locaux des objets sélectionnés (barres : divisions, longueur maximale,
//                        répartition, raffinement aux extrémités, portion ; surfaces : taille, divisions par
//                        direction, forme, structuré ; volumes : taille) — les autres objets ne changent pas ;
//   MeshQualityPanel     contrôle de qualité : synthèse et liste interactive des problèmes (sélection →
//                        localisation dans la vue 3D) ;
//   SlabOpeningsDialog   ouvertures (trémies) d'une dalle, dans son repère local, validées.

#include "../../Mesh/MeshQuality.h"
#include "../../Mesh/MeshTypes.h"
#include "../../Model/Slab.h"

#include <QDialog>
#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

class LocalMeshDialog : public QDialog
{
    Q_OBJECT

public:
    LocalMeshDialog(const std::vector<std::string>& refs, const TSA::Mesh::LocalMeshSetting* existing, QWidget* parent = nullptr);
    TSA::Mesh::LocalMeshSetting result() const;

private:
    std::vector<std::string> m_refs;
    bool m_members = false, m_surfaces = false, m_volumes = false;
    QLineEdit* m_name = nullptr;
    QCheckBox *m_useSize = nullptr, *m_useDiv = nullptr, *m_useMax = nullptr, *m_useEnd = nullptr, *m_usePortion = nullptr, *m_useUV = nullptr;
    QDoubleSpinBox *m_size = nullptr, *m_max = nullptr, *m_end = nullptr, *m_prog = nullptr, *m_pFrom = nullptr, *m_pTo = nullptr, *m_pSize = nullptr;
    QSpinBox *m_div = nullptr, *m_divU = nullptr, *m_divV = nullptr;
    QComboBox *m_dist = nullptr, *m_shape = nullptr, *m_structured = nullptr;
};

class MeshQualityPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MeshQualityPanel(QWidget* parent = nullptr);
    void setReport(const TSA::Mesh::QualityReport& report, const TSA::Mesh::FeMesh* mesh, const QString& context);

signals:
    void issueActivated(int element, int node);
    void convergenceRequested();

private:
    QLabel* m_summary = nullptr;
    QComboBox* m_filter = nullptr;
    QTableWidget* m_table = nullptr;
    TSA::Mesh::QualityReport m_report;
    void fill();
};

class SlabOpeningsDialog : public QDialog
{
    Q_OBJECT

public:
    SlabOpeningsDialog(const TSA::Model::Model& model, int slabId, QWidget* parent = nullptr);
    std::vector<TSA::Model::SlabOpening> openings() const { return m_openings; }

private:
    void fill();
    const TSA::Model::Model& m_model;
    int m_slabId;
    std::vector<TSA::Model::SlabOpening> m_openings;
    QTableWidget* m_table = nullptr;
};

} // namespace TSA::UI
