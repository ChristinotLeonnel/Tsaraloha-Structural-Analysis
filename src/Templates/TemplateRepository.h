#pragma once

// Dépôt des templates de documents (docs/TEMPLATES.md) : chaque paquet est classé par ORIGINE, affichée à
// l'utilisateur avec son état (modifié ou non) :
//   intégré       ressources de l'application (:/templates/builtin/<id>/), jamais modifié sur place
//   module        fourni par un module actif (ModuleRegistry → registerModulePackage)
//   utilisateur   <templates>/*.tsatemplate déposé ou créé par l'utilisateur
//   importé       <templates>/imported/ : copie validée d'un paquet importé (empreinte d'import mémorisée)
//   personnalisé  <templates>/custom/   : copie d'un paquet (basedOn, baseHash) modifiée par l'utilisateur
//   ancienne version <templates>/versions/ : versions remplacées par un import ou une personnalisation
// Un import ne remplace jamais silencieusement : la version précédente est archivée dans versions/.
// Pour un même identifiant, le paquet EFFECTIF est : personnalisé > importé > utilisateur > module > intégré.

#include "TemplatePackage.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace TSA::Templates
{

enum class TemplateOrigin
{
    BuiltIn,
    Module,
    User,
    Imported,
    Customized,
    Archived
};
QString templateOriginName(TemplateOrigin o);

struct TemplateEntry
{
    TemplatePackage package;
    TemplateOrigin origin = TemplateOrigin::BuiltIn;
    QString path;           ///< fichier ou dossier source
    QString moduleId;       ///< origine « module »
    bool modified = false;  ///< contenu différent de l'original (personnalisé) ou de la copie importée
    QString key() const;    ///< identifiant unique de l'entrée dans le dépôt (origine + chemin)
};

struct TemplateProblem
{
    QString path;
    QStringList errors;
};

class TemplateRepository
{
public:
    TemplateRepository();
    static TemplateRepository& instance();

    void setBuiltInRoot(const QString& root);   ///< défaut : « :/templates/builtin »
    void setUserRoot(const QString& root);      ///< défaut : AppPaths::userTemplatesDir()
    QString userRoot() const;

    /// Relit les paquets intégrés et utilisateur (les paquets des modules sont conservés).
    void reload();

    bool registerModulePackage(const QString& path, const QString& moduleId, QString* error = nullptr);
    void unregisterModule(const QString& moduleId);

    const std::vector<TemplateEntry>& entries() const { return m_entries; }
    const std::vector<TemplateProblem>& problems() const { return m_problems; }
    const TemplateEntry* entryByKey(const QString& key) const;
    /// Paquet effectif pour un identifiant (hors anciennes versions), nullptr si aucun.
    const TemplateEntry* effective(const QString& id) const;
    /// Entrées effectives proposant un rapport sur ce schéma de données (« tsa-report-data/1 »…).
    std::vector<const TemplateEntry*> effectiveEntries(const QString& dataSchema = {}) const;

    /// Importe un fichier .tsatemplate (validé ; version précédente du même id archivée). Rend la clé.
    bool importPackage(const QString& path, QString* key, QStringList* errors);
    bool exportPackage(const QString& key, const QString& path, QString* error = nullptr) const;
    /// Crée (ou remplace, en archivant) la copie personnalisée d'une entrée ; rend la nouvelle clé.
    bool saveCustomized(const QString& baseKey, const TemplatePackage& edited, QString* key, QStringList* errors);
    /// Supprime une entrée utilisateur, importée, personnalisée ou archivée (fichier supprimé).
    bool remove(const QString& key, QString* error = nullptr);

private:
    void scanDir(const QString& dir, TemplateOrigin origin);
    bool archive(const QString& file);
    QString importIndexPath() const;

    QString m_builtInRoot, m_userRoot;
    std::vector<TemplateEntry> m_entries;
    std::vector<TemplateEntry> m_moduleEntries;
    std::vector<TemplateProblem> m_problems;
};

} // namespace TSA::Templates
