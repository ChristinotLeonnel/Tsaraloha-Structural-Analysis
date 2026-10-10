#pragma once

// Pilotage du maillage du projet (docs/MESH.md) : paramètres et maillage du projet (Model::meshProjectJson),
// génération par Gmsh dans un thread de travail (interface jamais bloquée, progression, annulation entre
// deux étapes de Gmsh), aperçu distinct du maillage validé (un aperçu ne remplace jamais la génération ni
// son contrôle de qualité), détection d'un maillage obsolète, import / export, préparation de la
// transmission au solveur (adaptateur OpenSees).

#include "../../Mesh/MeshProject.h"
#include "../../Mesh/MeshQuality.h"
#include "../../Mesh/Solvers/OpenSeesMeshAdapter.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <optional>

class QThread;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

class MeshController : public QObject
{
    Q_OBJECT

public:
    explicit MeshController(QObject* parent = nullptr);
    ~MeshController() override;

    void setModel(TSA::Model::Model* model);   ///< relit le maillage enregistré dans le projet
    TSA::Model::Model* model() const { return m_model; }

    const TSA::Mesh::MeshProject& project() const { return m_project; }
    TSA::Mesh::MeshSettingsSet& settings() { return m_project.settings; }
    /// Enregistre les paramètres dans le projet (document modifié).
    void commitSettings();

    TSA::Mesh::MeshEstimate estimate() const;
    bool busy() const { return m_thread != nullptr; }
    /// Génération asynchrone : aperçu (non enregistré) ou maillage du projet (enregistré, qualité contrôlée).
    bool generate(bool preview);
    void cancel();
    const TSA::Mesh::FeMesh* preview() const { return m_preview ? &m_preview->mesh : nullptr; }
    const TSA::Mesh::GenerationRecord* previewRecord() const { return m_preview ? &m_preview->record : nullptr; }
    void discardPreview();

    bool hasMesh() const { return m_project.hasMesh; }
    bool stale() const;
    bool settingsChanged() const { return m_project.settingsChanged(); }
    const TSA::Mesh::QualityReport& quality() const { return m_quality; }
    void clearMesh();

    bool exportMesh(const QString& path, QString* error) const;
    bool importMesh(const QString& path, QStringList* warnings, QString* error);

    /// Subdivision à transmettre au moteur (vide si pas de maillage) et rapport de compatibilité.
    TSA::Mesh::SolverCompatibility solverCompatibility(const std::string& engineName, const TSA::Analysis::AnalysisCapabilities& caps) const;

signals:
    void generationStarted(bool preview);
    void progress(int percent, const QString& status);
    void generationFinished(bool preview, bool ok, const QString& message);
    void meshChanged();        ///< maillage du projet remplacé, supprimé ou relu
    void previewChanged();
    void settingsCommitted();

private:
    void recomputeQuality();

    TSA::Model::Model* m_model = nullptr;
    TSA::Mesh::MeshProject m_project;
    TSA::Mesh::QualityReport m_quality;
    std::optional<TSA::Mesh::GenerationResult> m_preview;
    QThread* m_thread = nullptr;
    std::shared_ptr<std::atomic<bool>> m_cancel;
};

/// Libellé d'état du maillage (« à jour », « obsolète », « aucun »…) pour la barre d'état et les panneaux.
QString meshStatusText(const MeshController& controller);

} // namespace TSA::UI
