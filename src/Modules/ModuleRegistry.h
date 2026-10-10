#pragma once

// Modules de l'écosystème Tsaraloha (TSA, TSALab) : contrat d'intégration (docs/SDK.md). Un module est un
// dossier contenant un manifeste « module.json » ; il peut fournir, selon ses capacités déclarées :
//   - un plugin binaire (DLL, API TSA::Plugins v1 : même compilateur et même runtime que l'hôte) ;
//   - des convertisseurs lancés en PROCESSUS SÉPARÉ (importers / exporters) : un exécutable qui lit un
//     fichier et écrit un fichier TSA3D (ou l'inverse) ; aucune contrainte d'ABI, tout langage ;
//   - des paquets de templates (.tsatemplate) : données, jamais de code exécutable.
//
// Sécurité : un module livré avec l'application (<app>/modules) est approuvé ; un module installé par
// l'utilisateur (<données>/modules) est découvert et validé mais N'EST PAS ACTIVÉ (ni plugin chargé, ni
// convertisseur exécuté) tant que l'utilisateur ne l'a pas approuvé (empreinte du manifeste mémorisée :
// une modification du module exige une nouvelle approbation).

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace TSA::Modules
{

struct Version
{
    int major = 0, minor = 0, patch = 0;
    static bool parse(const QString& text, Version* out);
    QString toString() const;
    bool operator<(const Version& o) const;
    bool operator==(const Version& o) const { return major == o.major && minor == o.minor && patch == o.patch; }
    bool operator<=(const Version& o) const { return *this < o || *this == o; }
};

struct ConverterSpec
{
    QString id;                ///< identifiant unique (préfixé par celui du module)
    QString title;
    QStringList extensions;    ///< extensions de fichier acceptées (importer) ou produites (exporter)
    QString command;           ///< exécutable relatif au dossier du module
    QStringList arguments;     ///< {input} et {output} remplacés
    QString moduleId;
    QString moduleDir;
};

struct ModuleManifest
{
    QString schema;            ///< « tsa-module/1 »
    QString id, name, description, publisher, license;
    Version version;
    struct Host
    {
        QString application;   ///< « TSA », « TSALab »
        Version min;
        bool hasMax = false;
        Version max;
    };
    std::vector<Host> hosts;
    struct Dependency
    {
        QString id;
        Version min;
        bool optional = false;
    };
    std::vector<Dependency> dependencies;
    QString plugin;            ///< DLL relative (facultatif)
    std::vector<ConverterSpec> importers, exporters;
    QStringList templates;     ///< paquets .tsatemplate relatifs
    QStringList resources;
    QStringList dataTypes;     ///< ex. « TSA3D/1 »
    QStringList capabilities;  ///< « plugin », « process », « templates » (déclarées, contrôlées)
    QJsonObject raw;

    /// Lecture + validation (structure, identifiants, versions, chemins confinés au dossier du module,
    /// fichiers présents, capacités cohérentes). Erreurs explicites dans *errors.
    static bool fromJson(const QJsonObject& json, const QString& moduleDir, ModuleManifest* out, QStringList* errors);
};

enum class ModuleState
{
    Invalid,            ///< manifeste illisible ou invalide
    Incompatible,       ///< hôte ou version non prise en charge
    MissingDependency,  ///< dépendance absente, incompatible ou elle-même inactive
    DependencyCycle,
    Untrusted,          ///< valide, en attente d'approbation (modules utilisateur)
    Disabled,           ///< désactivé par l'utilisateur
    Ready,              ///< valide, approuvé, non encore activé
    Active,
    Failed              ///< erreur à l'activation (plugin refusé…)
};
QString moduleStateName(ModuleState s);

struct ModuleInfo
{
    QString dir;
    QString manifestSha256;
    bool shipped = false;      ///< livré avec l'application (approuvé d'office)
    ModuleManifest manifest;
    ModuleState state = ModuleState::Invalid;
    QStringList messages;
};

/// Services fournis par l'hôte à l'activation (évite toute dépendance du registre envers l'interface,
/// le gestionnaire de templates ou le chargeur de plugins).
struct ModuleHostServices
{
    std::function<bool(const QString& packagePath, const QString& moduleId, QString* error)> registerTemplate;
    std::function<void(const QString& moduleId)> unregisterTemplates;
    std::function<bool(const QString& dllPath, QString* error)> loadPlugin;
    std::function<void(const QString& dllPath)> stopPlugin;
};

class ModuleRegistry
{
public:
    explicit ModuleRegistry(QString hostApplication = QStringLiteral("TSA"), QString hostVersion = QStringLiteral("0.1.0"));
    static ModuleRegistry& instance();
    void setHost(const QString& application, const QString& version);

    /// Fichier des approbations (défaut : <configuration utilisateur>/modules-trust.json).
    void setTrustFile(const QString& path);
    /// Découvre et valide les modules des dossiers ; les dossiers « shipped » sont approuvés d'office.
    void discover(const QStringList& shippedDirs, const QStringList& userDirs);
    const std::vector<ModuleInfo>& modules() const { return m_modules; }
    const ModuleInfo* module(const QString& id) const;

    bool approve(const QString& id, QString* error = nullptr);   ///< mémorise l'empreinte du manifeste
    bool setEnabled(const QString& id, bool enabled);

    /// Active les modules prêts, dans l'ordre des dépendances. Rend le nombre de modules actifs.
    int activate(const ModuleHostServices& services);
    /// Arrête les modules actifs (ordre inverse) : templates et convertisseurs retirés, plugin notifié.
    void shutdown(const ModuleHostServices& services);

    /// Convertisseurs des modules actifs.
    std::vector<ConverterSpec> importers() const;
    std::vector<ConverterSpec> exporters() const;
    /// Exécute un convertisseur dans un processus séparé (dossier de travail = dossier du module).
    bool runConverter(const ConverterSpec& c, const QString& input, const QString& output, QString* log, int timeoutMs = 120000) const;

private:
    void resolveDependencies();
    QStringList loadTrust() const;
    bool saveTrust(const QStringList& entries) const;

    QString m_hostApp, m_hostVersion, m_trustFile;
    std::vector<ModuleInfo> m_modules;
    QStringList m_disabled;
    QStringList m_activationOrder;   ///< ordre réel d'activation (arrêt dans l'ordre inverse)
};

} // namespace TSA::Modules
