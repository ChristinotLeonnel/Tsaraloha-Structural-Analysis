#pragma once

// Maillage d'un projet : paramètres (globaux, locaux, zones, profil), dernier maillage généré et son
// rapport (mailleur, version, empreintes, avertissements, interfaces, synthèse qualité). Persisté dans le
// chunk MESH du .tsa (Model::meshProjectJson). Profils : bibliothèque utilisateur + profils de départ.

#include "MeshGenerator.h"

#include <QJsonObject>
#include <QString>

namespace TSA::Model
{
class Model;
}

namespace TSA::Mesh
{

inline constexpr char kMeshProjectSchema[] = "tsa-mesh/1";

struct QualitySummary
{
    bool available = false;
    int errors = 0, warnings = 0;
    double worstAspect = 0, minAngle = 0, maxAngle = 0, minJacobian = 0;
    std::size_t estimatedDofs = 0;
};

struct MeshProject
{
    MeshSettingsSet settings;
    bool hasMesh = false;
    FeMesh mesh;
    GenerationRecord record;
    QualitySummary quality;

    QJsonObject toJson(bool includeMesh = true) const;
    static MeshProject fromJson(const QJsonObject& json, QStringList* warnings = nullptr);

    static MeshProject load(const TSA::Model::Model& model);
    void store(TSA::Model::Model& model) const;

    /// Maillage généré pour une autre géométrie (le modèle a changé depuis).
    bool isStale(const std::string& currentGeometryHash) const { return hasMesh && record.geometryHash != currentGeometryHash; }
    /// Paramètres modifiés depuis la génération.
    bool settingsChanged() const;
};

// Sérialisation des éléments (réutilisée par les profils)
QJsonObject globalSettingsToJson(const GlobalMeshSettings& g);
GlobalMeshSettings globalSettingsFromJson(const QJsonObject& o);
QJsonObject localSettingToJson(const LocalMeshSetting& l);
LocalMeshSetting localSettingFromJson(const QJsonObject& o);
QJsonObject zoneToJson(const RefinementZone& z);
RefinementZone zoneFromJson(const QJsonObject& o);
QJsonObject meshToJson(const FeMesh& m);
FeMesh meshFromJson(const QJsonObject& o);

// --- Profils ---------------------------------------------------------------------------------------

/// Profils de départ (modifiables par copie) : rapide, standard, fin, très fin, personnalisé, et profils
/// spécialisés (poutres, dalles, voiles, ponts, volumes).
std::vector<MeshProfile> builtInProfiles();

class MeshProfileLibrary
{
public:
    explicit MeshProfileLibrary(QString file = {});   ///< défaut : <configuration utilisateur>/mesh-profiles.json
    std::vector<MeshProfile> all() const;              ///< départ + utilisateur (un profil utilisateur du même nom remplace celui de départ)
    const std::vector<MeshProfile>& user() const { return m_user; }
    std::optional<MeshProfile> find(const std::string& name) const;
    bool save(const MeshProfile& profile, QString* error = nullptr);   ///< crée ou remplace un profil utilisateur
    bool remove(const std::string& name, QString* error = nullptr);
    bool exportProfile(const std::string& name, const QString& path, QString* error = nullptr) const;
    bool importProfile(const QString& path, std::string* name, QString* error = nullptr);
    QString file() const { return m_file; }

private:
    bool write(QString* error) const;
    QString m_file;
    std::vector<MeshProfile> m_user;
};

QJsonObject profileToJson(const MeshProfile& p);
MeshProfile profileFromJson(const QJsonObject& o);

} // namespace TSA::Mesh
