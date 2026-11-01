#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Project
{

/// Fichier relié au projet : source d'un import, fichier exporté, texture de matériau, document joint.
struct LinkedFile
{
    std::string role;            ///< "import-ifc", "import-tsa3d", "export-ifc", "export-tsa3d", "texture", "attachment"
    std::string relativePath;    ///< relatif au dossier du .tsa (séparateurs « / ») ; vide si non exprimable
    std::string absolutePath;    ///< repli ; vide si l'utilisateur ne veut pas de chemins absolus dans le fichier (D5)
    std::int64_t size = 0;
    std::string sha256;          ///< empreinte du fichier au moment de l'enregistrement du lien
    std::string modified;        ///< date de modification du fichier (ISO 8601, UTC)

    bool operator==(const LinkedFile&) const = default;
};

enum class LinkStatus
{
    Present,    ///< trouvé, identique
    Modified,   ///< trouvé, contenu différent depuis l'enregistrement du lien
    Missing     ///< introuvable (chemin relatif puis absolu)
};

struct LinkCheck
{
    LinkedFile link;
    LinkStatus status = LinkStatus::Missing;
    std::string resolvedPath;    ///< chemin où le fichier a été trouvé
};

/// Bibliothèque TSALib dont le projet utilise des définitions (DefinitionReference des éléments).
struct LibraryRequirement
{
    std::string id;
    std::string version;
    std::vector<std::string> definitions;   ///< identifiants des définitions utilisées (triés)

    bool operator==(const LibraryRequirement&) const = default;
};

struct LibraryCheck
{
    LibraryRequirement requirement;
    bool installed = false;
    std::string installedVersion;   ///< vide si absente
    bool sameVersion() const { return installed && installedVersion == requirement.version; }
};

struct SaveRecord
{
    std::string timestamp;      ///< ISO 8601, UTC
    std::string author;
    std::string application;    ///< produit et version (« TSA 0.1.0 »)
    std::string format;         ///< version du format .tsa (« 3.1 »)

    bool operator==(const SaveRecord&) const = default;
};

struct ProjectProperties
{
    std::string description, client, reference, phase;
    bool operator==(const ProjectProperties&) const = default;
};

/// Manifeste du projet (bloc PMAN du .tsa, docs/PROJECT_STATE_PLAN.md §3.5) : bibliothèques requises, fichiers liés
/// (contrôlés à l'ouverture, jamais ouverts ni exécutés), historique des enregistrements, propriétés libres.
class ProjectManifest
{
public:
    static constexpr std::size_t kMaxSaves = 50;

    // --- Fichiers liés ------------------------------------------------------------------------------------------------
    /// Ajoute (ou met à jour : même rôle et même chemin) un fichier existant ; taille, empreinte et date relevées
    /// maintenant. Faux si le fichier n'existe pas. projectFile : .tsa courant (vide : projet sans fichier, chemin
    /// relatif calculé au prochain enregistrement).
    bool registerLink(const std::string& role, const std::string& absolutePath, const std::string& projectFile = {});
    bool removeLink(const std::string& role, const std::string& path);
    const std::vector<LinkedFile>& links() const noexcept { return m_links; }
    /// À l'enregistrement : chemins relatifs recalculés pour le nouvel emplacement du .tsa (Enregistrer sous) ; chemins
    /// absolus conservés ou effacés selon la préférence de l'utilisateur (D5).
    void updateLinkPaths(const std::string& projectFile, bool keepAbsolutePaths);
    /// Présence et intégrité de chaque fichier lié (chemin relatif au .tsa, puis chemin absolu).
    std::vector<LinkCheck> checkLinks(const std::string& projectFile) const;

    // --- Bibliothèques ------------------------------------------------------------------------------------------------
    /// Bibliothèques requises, relevées dans les références de définitions du modèle (remplace la liste).
    void collectLibraries(const TSA::Model::Model& model);
    const std::vector<LibraryRequirement>& libraries() const noexcept { return m_libraries; }
    /// installed : (identifiant, version) des bibliothèques installées sur ce poste.
    std::vector<LibraryCheck> checkLibraries(const std::vector<std::pair<std::string, std::string>>& installed) const;

    // --- Enregistrements et propriétés ---------------------------------------------------------------------------------
    void recordSave(const SaveRecord& record);
    const std::deque<SaveRecord>& saves() const noexcept { return m_saves; }
    const ProjectProperties& properties() const noexcept { return m_properties; }
    void setProperties(const ProjectProperties& p) { m_properties = p; }

    bool empty() const noexcept;
    void clear();

    /// JSON UTF-8 « schema » 1 ; fromJson : faux (objet inchangé, *error renseigné) si illisible ou d'une version inconnue.
    std::string toJson() const;
    bool fromJson(const std::string& json, std::string* error = nullptr);

    bool operator==(const ProjectManifest&) const = default;

private:
    std::vector<LinkedFile> m_links;
    std::vector<LibraryRequirement> m_libraries;
    std::deque<SaveRecord> m_saves;   ///< du plus ancien au plus récent
    ProjectProperties m_properties;
};

/// Libellé lisible d'un rôle de fichier lié (« Source de l'import IFC »…).
std::string linkRoleLabel(const std::string& role);

} // namespace TSA::Project
