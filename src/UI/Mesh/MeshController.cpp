#include "MeshController.h"

#include "../../Model/Model.h"

#include <QDateTime>
#include <QPointer>
#include <QThread>

#include <cmath>

namespace TSA::UI
{

using namespace TSA::Mesh;

MeshController::MeshController(QObject* parent)
    : QObject(parent)
{
}

MeshController::~MeshController()
{
    if (m_thread)
    {
        if (m_cancel) *m_cancel = true;
        m_thread->wait();
    }
}

void MeshController::setModel(TSA::Model::Model* model)
{
    m_model = model;
    m_project = model ? MeshProject::load(*model) : MeshProject {};
    m_preview.reset();
    recomputeQuality();
    emit meshChanged();
    emit previewChanged();
}

void MeshController::commitSettings()
{
    if (!m_model) return;
    m_project.store(*m_model);
    m_model->setModified(true);
    emit settingsCommitted();
}

MeshEstimate MeshController::estimate() const
{
    if (!m_model) return {};
    return MeshGenerator::estimate(MeshGeometry::extract(*m_model), m_project.settings);
}

bool MeshController::stale() const
{
    return m_model && m_project.hasMesh && m_project.isStale(MeshGeometry::extract(*m_model).hash);
}

void MeshController::recomputeQuality()
{
    m_quality = m_project.hasMesh ? analyzeMesh(m_project.mesh, m_project.settings.global.quality, m_project.record.interfaces) : QualityReport {};
    if (m_project.hasMesh)
    {
        m_project.quality.available = true;
        m_project.quality.errors = m_quality.errors;
        m_project.quality.warnings = m_quality.warnings;
        m_project.quality.worstAspect = m_quality.aspect.max;
        m_project.quality.minAngle = m_quality.minAngle.min;
        m_project.quality.maxAngle = m_quality.maxAngle.max;
        m_project.quality.minJacobian = m_quality.jacobian.min;
        m_project.quality.estimatedDofs = m_quality.estimatedDofs;
    }
}

bool MeshController::generate(bool preview)
{
    if (!m_model || m_thread) return false;
    // Géométrie et paramètres copiés dans le thread d'interface : le thread de travail ne lit jamais le modèle.
    auto geometry = std::make_shared<MeshGeometry>(MeshGeometry::extract(*m_model));
    auto settings = std::make_shared<MeshSettingsSet>(m_project.settings);
    auto result = std::make_shared<GenerationResult>();
    m_cancel = std::make_shared<std::atomic<bool>>(false);
    auto cancel = m_cancel;
    QPointer<MeshController> self(this);
    m_thread = QThread::create([geometry, settings, result, cancel, self] {
        GenerationOptions o;
        o.cancelled = [cancel] { return cancel->load(); };
        o.progress = [self](int p, const std::string& s) {
            if (self) QMetaObject::invokeMethod(self, [self, p, s] { if (self) emit self->progress(p, QString::fromStdString(s)); }, Qt::QueuedConnection);
        };
        *result = MeshGenerator::generate(*geometry, *settings, o);
    });
    connect(m_thread, &QThread::finished, this, [this, result, preview] {
        m_thread->deleteLater();
        m_thread = nullptr;
        QString message;
        if (!result->ok)
        {
            QStringList e;
            for (const auto& x : result->record.errors) e << QString::fromStdString(x);
            message = e.join('\n');
            emit generationFinished(preview, false, message);
            return;
        }
        message = tr("%1 nœuds, %2 éléments (1D : %3, 2D : %4, 3D : %5) en %6 ms")
                      .arg(result->mesh.nodes.size())
                      .arg(result->mesh.countByDimension(1) + result->mesh.countByDimension(2) + result->mesh.countByDimension(3))
                      .arg(result->mesh.countByDimension(1))
                      .arg(result->mesh.countByDimension(2))
                      .arg(result->mesh.countByDimension(3))
                      .arg(result->record.durationMs, 0, 'f', 0);
        if (preview)
        {
            m_preview = *result;
            emit previewChanged();
        }
        else
        {
            m_project.hasMesh = true;
            m_project.mesh = std::move(result->mesh);
            m_project.record = std::move(result->record);
            m_preview.reset();
            recomputeQuality();
            commitSettings();
            emit previewChanged();
            emit meshChanged();
        }
        emit generationFinished(preview, true, message);
    });
    emit generationStarted(preview);
    m_thread->start();
    return true;
}

void MeshController::cancel()
{
    if (m_cancel) *m_cancel = true;
}

void MeshController::discardPreview()
{
    m_preview.reset();
    emit previewChanged();
}

void MeshController::clearMesh()
{
    m_project.hasMesh = false;
    m_project.mesh = {};
    m_project.record = {};
    m_project.quality = {};
    recomputeQuality();
    commitSettings();
    emit meshChanged();
}

bool MeshController::exportMesh(const QString& path, QString* error) const
{
    if (!m_project.hasMesh)
    {
        if (error) *error = tr("Aucun maillage généré.");
        return false;
    }
    std::string err;
    const bool ok = MeshGenerator::exportMesh(m_project.mesh, path.toStdString(), &err);
    if (!ok && error) *error = QString::fromStdString(err);
    return ok;
}

bool MeshController::importMesh(const QString& path, QStringList* warnings, QString* error)
{
    if (!m_model) return false;
    FeMesh mesh;
    std::vector<std::string> warn;
    std::string err;
    if (!MeshGenerator::importMesh(path.toStdString(), &mesh, &warn, &err))
    {
        if (error) *error = QString::fromStdString(err);
        return false;
    }
    // Rattachement aux nœuds TSA par la position ; correspondance avec la géométrie non garantie : le maillage
    // importé est marqué comme tel (il sera obsolète au premier contrôle si ses objets ne sont pas ceux du modèle).
    const auto g = MeshGeometry::extract(*m_model);
    std::size_t linked = 0;
    for (auto& n : mesh.nodes)
        for (const auto& [id, p] : g.tsaNodes)
            if (std::abs(p.x - n.x) < 1e-6 && std::abs(p.y - n.y) < 1e-6 && std::abs(p.z - n.z) < 1e-6)
            {
                n.tsaNode = id;
                ++linked;
                break;
            }
    std::size_t known = 0, total = 0;
    for (const auto& e : mesh.elements)
    {
        ++total;
        known += (g.member(e.source) || g.surface(e.source) || g.volume(e.source)) ? 1 : 0;
    }
    m_project.hasMesh = true;
    m_project.mesh = std::move(mesh);
    m_project.record = {};
    m_project.record.mesher = "Gmsh (import)";
    m_project.record.generated = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    m_project.record.parameters["fichier importé"] = path.toStdString();
    m_project.record.geometryHash = known == total ? g.hash : std::string("import");
    m_project.record.settingsHash = MeshGenerator::settingsHash(m_project.settings);
    for (const auto& w : warn) m_project.record.warnings.push_back(w);
    if (known < total) m_project.record.warnings.push_back(std::to_string(total - known) + " élément(s) rattaché(s) à des objets absents du modèle");
    if (warnings)
    {
        for (const auto& w : m_project.record.warnings) warnings->append(QString::fromStdString(w));
        warnings->append(tr("%1 nœud(s) rattaché(s) aux nœuds du modèle par leur position").arg(linked));
    }
    recomputeQuality();
    commitSettings();
    emit meshChanged();
    return true;
}

SolverCompatibility MeshController::solverCompatibility(const std::string& engineName, const TSA::Analysis::AnalysisCapabilities& caps) const
{
    if (!m_model || !m_project.hasMesh) return {};
    return OpenSeesMeshAdapter::check(m_project.mesh, MeshGeometry::extract(*m_model), engineName, caps);
}

QString meshStatusText(const MeshController& c)
{
    if (!meshEngineCapabilities().available) return QObject::tr("Maillage indisponible : %1").arg(QString::fromStdString(meshEngineCapabilities().unavailableReason));
    if (c.busy()) return QObject::tr("Maillage : génération en cours…");
    if (!c.hasMesh()) return QObject::tr("Maillage : aucun (un élément fini par barre au calcul)");
    const auto& m = c.project().mesh;
    QString s = QObject::tr("Maillage : %1 nœuds, %2 éléments").arg(m.nodes.size()).arg(m.countByDimension(1) + m.countByDimension(2) + m.countByDimension(3));
    if (c.stale()) s += QObject::tr(" — OBSOLÈTE (géométrie modifiée depuis la génération)");
    else if (c.settingsChanged()) s += QObject::tr(" — paramètres modifiés depuis la génération");
    else s += QObject::tr(" — à jour");
    if (c.quality().errors) s += QObject::tr(" — %1 erreur(s) de qualité").arg(c.quality().errors);
    return s;
}

} // namespace TSA::UI
