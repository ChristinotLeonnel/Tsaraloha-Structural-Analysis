#include "ModuleRegistry.h"

#include "../Core/AppPaths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <map>
#include <set>

namespace TSA::Modules
{

// --- Versions ---------------------------------------------------------------------------------------

bool Version::parse(const QString& text, Version* out)
{
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\.(\\d+)(?:\\.(\\d+))?$"));
    const auto m = re.match(text.trimmed());
    if (!m.hasMatch()) return false;
    if (out) *out = { m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt() };
    return true;
}

QString Version::toString() const
{
    return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

bool Version::operator<(const Version& o) const
{
    if (major != o.major) return major < o.major;
    if (minor != o.minor) return minor < o.minor;
    return patch < o.patch;
}

QString moduleStateName(ModuleState s)
{
    switch (s)
    {
    case ModuleState::Invalid: return QStringLiteral("invalide");
    case ModuleState::Incompatible: return QStringLiteral("incompatible");
    case ModuleState::MissingDependency: return QStringLiteral("dépendance manquante");
    case ModuleState::DependencyCycle: return QStringLiteral("dépendances circulaires");
    case ModuleState::Untrusted: return QStringLiteral("à approuver");
    case ModuleState::Disabled: return QStringLiteral("désactivé");
    case ModuleState::Ready: return QStringLiteral("prêt");
    case ModuleState::Active: return QStringLiteral("actif");
    case ModuleState::Failed: return QStringLiteral("échec");
    }
    return {};
}

// --- Manifeste --------------------------------------------------------------------------------------

namespace
{
/// Chemin relatif confiné au dossier du module (ni absolu, ni « .. »), fichier présent.
bool confinedFile(const QString& moduleDir, const QString& rel, const QString& field, QStringList* errors, bool mustExist = true)
{
    if (rel.isEmpty() || QDir::isAbsolutePath(rel) || rel.contains(QStringLiteral("..")) || rel.contains(QLatin1Char(':')))
    {
        errors->append(QStringLiteral("%1 : chemin « %2 » refusé (relatif au dossier du module, sans « .. »)").arg(field, rel));
        return false;
    }
    if (mustExist && !QFileInfo::exists(QDir(moduleDir).filePath(rel)))
    {
        errors->append(QStringLiteral("%1 : fichier « %2 » absent du module").arg(field, rel));
        return false;
    }
    return true;
}

std::vector<ConverterSpec> converters(const QJsonArray& arr, const QString& field, const QString& moduleId, const QString& dir, QStringList* errors)
{
    std::vector<ConverterSpec> out;
    for (int i = 0; i < arr.size(); ++i)
    {
        const QJsonObject o = arr[i].toObject();
        const QString path = QStringLiteral("%1[%2]").arg(field).arg(i);
        ConverterSpec c;
        c.id = o.value("id").toString();
        c.title = o.value("title").toString(c.id);
        c.command = o.value("command").toString();
        c.moduleId = moduleId;
        c.moduleDir = dir;
        for (const auto& e : o.value("extensions").toArray()) c.extensions << e.toString().toLower();
        for (const auto& a : o.value("arguments").toArray()) c.arguments << a.toString();
        if (c.arguments.isEmpty()) c.arguments = { QStringLiteral("{input}"), QStringLiteral("{output}") };
        if (!c.id.startsWith(moduleId + QLatin1Char('.')))
            errors->append(QStringLiteral("%1.id : « %2 » doit être préfixé par l'identifiant du module (%3.)").arg(path, c.id, moduleId));
        if (c.extensions.isEmpty()) errors->append(path + QStringLiteral(".extensions : au moins une extension attendue"));
        if (!c.arguments.join(' ').contains(QStringLiteral("{input}")) || !c.arguments.join(' ').contains(QStringLiteral("{output}")))
            errors->append(path + QStringLiteral(".arguments : {input} et {output} obligatoires"));
        confinedFile(dir, c.command, path + QStringLiteral(".command"), errors);
        out.push_back(c);
    }
    return out;
}
} // namespace

bool ModuleManifest::fromJson(const QJsonObject& j, const QString& dir, ModuleManifest* out, QStringList* errors)
{
    QStringList errs;
    ModuleManifest m;
    m.raw = j;
    m.schema = j.value("schema").toString();
    if (m.schema != QLatin1String("tsa-module/1")) errs << QStringLiteral("schema : « tsa-module/1 » attendu (lu « %1 »)").arg(m.schema);
    m.id = j.value("id").toString();
    static const QRegularExpression idRe(QStringLiteral("^[a-z0-9]+(\\.[a-z0-9_-]+)+$"));
    if (!idRe.match(m.id).hasMatch()) errs << QStringLiteral("id : « %1 » invalide (ex. « societe.outil », minuscules)").arg(m.id);
    m.name = j.value("name").toString();
    if (m.name.isEmpty()) errs << QStringLiteral("name : obligatoire");
    if (!Version::parse(j.value("version").toString(), &m.version)) errs << QStringLiteral("version : « majeur.mineur[.correctif] » attendu");
    m.description = j.value("description").toString();
    m.publisher = j.value("publisher").toString();
    m.license = j.value("license").toString();

    const QJsonArray hosts = j.value("hosts").toArray();
    if (hosts.isEmpty()) errs << QStringLiteral("hosts : au moins une application hôte (TSA, TSALab) attendue");
    for (int i = 0; i < hosts.size(); ++i)
    {
        const QJsonObject h = hosts[i].toObject();
        Host host;
        host.application = h.value("application").toString();
        if (host.application != "TSA" && host.application != "TSALab") errs << QStringLiteral("hosts[%1].application : TSA ou TSALab").arg(i);
        if (!Version::parse(h.value("min").toString(), &host.min)) errs << QStringLiteral("hosts[%1].min : version attendue").arg(i);
        if (h.contains("max"))
        {
            host.hasMax = Version::parse(h.value("max").toString(), &host.max);
            if (!host.hasMax) errs << QStringLiteral("hosts[%1].max : version attendue").arg(i);
            else if (host.max < host.min) errs << QStringLiteral("hosts[%1] : max < min").arg(i);
        }
        m.hosts.push_back(host);
    }
    for (const auto& v : j.value("dependencies").toArray())
    {
        const QJsonObject d = v.toObject();
        Dependency dep;
        dep.id = d.value("id").toString();
        dep.optional = d.value("optional").toBool();
        if (dep.id.isEmpty() || !Version::parse(d.value("min").toString("0.0.0"), &dep.min)) errs << QStringLiteral("dependencies : « id » et « min » attendus");
        if (dep.id == m.id) errs << QStringLiteral("dependencies : un module ne dépend pas de lui-même");
        m.dependencies.push_back(dep);
    }
    const QJsonObject ep = j.value("entryPoints").toObject();
    m.plugin = ep.value("plugin").toString();
    if (!m.plugin.isEmpty()) confinedFile(dir, m.plugin, QStringLiteral("entryPoints.plugin"), &errs);
    m.importers = converters(ep.value("importers").toArray(), QStringLiteral("entryPoints.importers"), m.id, dir, &errs);
    m.exporters = converters(ep.value("exporters").toArray(), QStringLiteral("entryPoints.exporters"), m.id, dir, &errs);
    for (const auto& v : j.value("templates").toArray())
    {
        m.templates << v.toString();
        confinedFile(dir, v.toString(), QStringLiteral("templates"), &errs);
    }
    for (const auto& v : j.value("resources").toArray())
    {
        m.resources << v.toString();
        confinedFile(dir, v.toString(), QStringLiteral("resources"), &errs);
    }
    for (const auto& v : j.value("dataTypes").toArray()) m.dataTypes << v.toString();
    for (const auto& v : j.value("capabilities").toArray()) m.capabilities << v.toString();
    // Capacités déclarées = ce que le module utilise réellement (rien d'implicite).
    auto needCapability = [&](bool used, const char* cap) {
        if (used && !m.capabilities.contains(QLatin1String(cap)))
            errs << QStringLiteral("capabilities : « %1 » doit être déclarée").arg(QLatin1String(cap));
    };
    needCapability(!m.plugin.isEmpty(), "plugin");
    needCapability(!m.importers.empty() || !m.exporters.empty(), "process");
    needCapability(!m.templates.isEmpty(), "templates");
    for (const QString& c : m.capabilities)
        if (c != "plugin" && c != "process" && c != "templates") errs << QStringLiteral("capabilities : « %1 » inconnue").arg(c);

    if (errors) *errors = errs;
    if (out) *out = m;
    return errs.isEmpty();
}

// --- Registre ---------------------------------------------------------------------------------------

ModuleRegistry::ModuleRegistry(QString hostApplication, QString hostVersion)
    : m_hostApp(std::move(hostApplication))
    , m_hostVersion(std::move(hostVersion))
{
}

ModuleRegistry& ModuleRegistry::instance()
{
    static ModuleRegistry r;
    return r;
}

void ModuleRegistry::setHost(const QString& application, const QString& version)
{
    m_hostApp = application;
    m_hostVersion = version;
}

void ModuleRegistry::setTrustFile(const QString& path)
{
    m_trustFile = path;
}

const ModuleInfo* ModuleRegistry::module(const QString& id) const
{
    for (const auto& m : m_modules)
        if (m.manifest.id == id) return &m;
    return nullptr;
}

QStringList ModuleRegistry::loadTrust() const
{
    QFile f(m_trustFile.isEmpty() ? TSA::Core::AppPaths::userConfigDir() + QStringLiteral("/modules-trust.json") : m_trustFile);
    QStringList out;
    if (!f.open(QIODevice::ReadOnly)) return out;
    for (const auto& v : QJsonDocument::fromJson(f.readAll()).object().value("approved").toArray()) out << v.toString();
    return out;
}

bool ModuleRegistry::saveTrust(const QStringList& entries) const
{
    const QString path = m_trustFile.isEmpty() ? TSA::Core::AppPaths::userConfigDir() + QStringLiteral("/modules-trust.json") : m_trustFile;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    QJsonArray arr;
    for (const auto& e : entries) arr.append(e);
    return f.open(QIODevice::WriteOnly) && f.write(QJsonDocument(QJsonObject { { "approved", arr } }).toJson()) >= 0 && f.commit();
}

void ModuleRegistry::discover(const QStringList& shippedDirs, const QStringList& userDirs)
{
    m_modules.clear();
    m_activationOrder.clear();   // (appeler shutdown() avant une nouvelle découverte)
    const QStringList trust = loadTrust();
    auto scan = [&](const QString& root, bool shipped) {
        if (root.isEmpty() || !QFileInfo(root).isDir()) return;
        for (const QFileInfo& d : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        {
            const QString manifestPath = d.absoluteFilePath() + QStringLiteral("/module.json");
            if (!QFileInfo::exists(manifestPath)) continue;
            ModuleInfo info;
            info.dir = d.absoluteFilePath();
            info.shipped = shipped;
            QFile f(manifestPath);
            QByteArray bytes;
            if (f.open(QIODevice::ReadOnly)) bytes = f.readAll();
            info.manifestSha256 = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
            QJsonParseError pe;
            const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
            QStringList errs;
            if (pe.error != QJsonParseError::NoError || !doc.isObject())
                errs << QStringLiteral("module.json illisible : %1").arg(pe.errorString());
            else
                ModuleManifest::fromJson(doc.object(), info.dir, &info.manifest, &errs);
            if (info.manifest.id.isEmpty()) info.manifest.id = d.fileName();
            if (!errs.isEmpty())
            {
                info.state = ModuleState::Invalid;
                info.messages = errs;
            }
            else
            {
                // Compatibilité avec l'hôte
                Version hv;
                Version::parse(m_hostVersion, &hv);
                bool compatible = false;
                QString why = QStringLiteral("aucune entrée pour %1").arg(m_hostApp);
                for (const auto& h : info.manifest.hosts)
                    if (h.application == m_hostApp)
                    {
                        compatible = h.min <= hv && (!h.hasMax || hv <= h.max);
                        why = QStringLiteral("%1 %2 requis entre %3 et %4").arg(m_hostApp, hv.toString(), h.min.toString(),
                                                                              h.hasMax ? h.max.toString() : QStringLiteral("∞"));
                    }
                if (!compatible)
                {
                    info.state = ModuleState::Incompatible;
                    info.messages << QStringLiteral("hôte incompatible : %1").arg(why);
                }
                else if (m_disabled.contains(info.manifest.id))
                    info.state = ModuleState::Disabled;
                else if (!shipped && !trust.contains(info.manifest.id + QLatin1Char('@') + info.manifestSha256))
                {
                    info.state = ModuleState::Untrusted;
                    info.messages << QStringLiteral("module installé par l'utilisateur : approbation requise avant activation");
                }
                else
                    info.state = ModuleState::Ready;
            }
            // Identifiant en double : le premier (livré) l'emporte.
            if (module(info.manifest.id))
            {
                info.state = ModuleState::Invalid;
                info.messages << QStringLiteral("identifiant « %1 » déjà fourni par un autre module").arg(info.manifest.id);
            }
            m_modules.push_back(info);
        }
    };
    for (const auto& d : shippedDirs) scan(d, true);
    for (const auto& d : userDirs) scan(d, false);
    resolveDependencies();
}

void ModuleRegistry::resolveDependencies()
{
    // Cycles (parcours en profondeur), puis propagation des dépendances inactives jusqu'à stabilité.
    std::map<QString, int> color;
    std::function<bool(const QString&)> cyclic = [&](const QString& id) -> bool {
        color[id] = 1;
        const auto* m = module(id);
        if (m)
            for (const auto& d : m->manifest.dependencies)
            {
                if (color[d.id] == 1) return true;
                if (color[d.id] == 0 && module(d.id) && cyclic(d.id)) return true;
            }
        color[id] = 2;
        return false;
    };
    for (auto& m : m_modules)
    {
        color.clear();
        if (m.state != ModuleState::Invalid && cyclic(m.manifest.id))
        {
            m.state = ModuleState::DependencyCycle;
            m.messages << QStringLiteral("dépendances circulaires");
        }
    }
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (auto& m : m_modules)
        {
            if (m.state != ModuleState::Ready && m.state != ModuleState::Untrusted) continue;
            for (const auto& d : m.manifest.dependencies)
            {
                const auto* dep = module(d.id);
                QString problem;
                if (!dep) problem = QStringLiteral("« %1 » absent").arg(d.id);
                else if (dep->manifest.version < d.min)
                    problem = QStringLiteral("« %1 » %2 présent, %3 requis").arg(d.id, dep->manifest.version.toString(), d.min.toString());
                else if (dep->state != ModuleState::Ready && dep->state != ModuleState::Active && dep->state != ModuleState::Untrusted)
                    problem = QStringLiteral("« %1 » inactif (%2)").arg(d.id, moduleStateName(dep->state));
                if (!problem.isEmpty() && !d.optional)
                {
                    m.state = ModuleState::MissingDependency;
                    m.messages << QStringLiteral("dépendance : %1").arg(problem);
                    changed = true;
                    break;
                }
            }
        }
    }
}

bool ModuleRegistry::approve(const QString& id, QString* error)
{
    for (auto& m : m_modules)
        if (m.manifest.id == id)
        {
            if (m.state != ModuleState::Untrusted)
            {
                if (error) *error = QStringLiteral("module « %1 » non approuvable (%2)").arg(id, moduleStateName(m.state));
                return false;
            }
            QStringList trust = loadTrust();
            trust << id + QLatin1Char('@') + m.manifestSha256;
            if (!saveTrust(trust))
            {
                if (error) *error = QStringLiteral("écriture des approbations impossible");
                return false;
            }
            m.state = ModuleState::Ready;
            m.messages.removeAll(QStringLiteral("module installé par l'utilisateur : approbation requise avant activation"));
            resolveDependencies();
            return true;
        }
    if (error) *error = QStringLiteral("module « %1 » inconnu").arg(id);
    return false;
}

bool ModuleRegistry::setEnabled(const QString& id, bool enabled)
{
    if (enabled) m_disabled.removeAll(id);
    else if (!m_disabled.contains(id)) m_disabled << id;
    for (auto& m : m_modules)
        if (m.manifest.id == id)
        {
            if (!enabled && (m.state == ModuleState::Ready || m.state == ModuleState::Untrusted)) m.state = ModuleState::Disabled;
            return true;
        }
    return false;
}

int ModuleRegistry::activate(const ModuleHostServices& services)
{
    // Ordre : un module après ses dépendances (les modules prêts n'ont pas de cycle).
    std::set<QString> done;
    int active = 0;
    bool progress = true;
    while (progress)
    {
        progress = false;
        for (auto& m : m_modules)
        {
            if (m.state != ModuleState::Ready) continue;
            bool depsDone = true;
            for (const auto& d : m.manifest.dependencies)
                if (!d.optional && !done.count(d.id)) depsDone = false;
            if (!depsDone) continue;
            QStringList errs;
            for (const QString& t : m.manifest.templates)
            {
                QString e;
                if (services.registerTemplate && !services.registerTemplate(QDir(m.dir).filePath(t), m.manifest.id, &e))
                    errs << QStringLiteral("template %1 : %2").arg(t, e);
            }
            if (!m.manifest.plugin.isEmpty())
            {
                QString e;
                if (!services.loadPlugin) errs << QStringLiteral("plugin : chargement non pris en charge par l'hôte");
                else if (!services.loadPlugin(QDir(m.dir).filePath(m.manifest.plugin), &e)) errs << QStringLiteral("plugin : %1").arg(e);
            }
            if (errs.isEmpty())
            {
                m.state = ModuleState::Active;
                m_activationOrder << m.manifest.id;
                done.insert(m.manifest.id);
                ++active;
            }
            else
            {
                if (services.unregisterTemplates) services.unregisterTemplates(m.manifest.id);
                m.state = ModuleState::Failed;
                m.messages << errs;
            }
            progress = true;
        }
    }
    return active;
}

void ModuleRegistry::shutdown(const ModuleHostServices& services)
{
    for (auto id = m_activationOrder.crbegin(); id != m_activationOrder.crend(); ++id)
        for (auto& m : m_modules)
        {
            if (m.manifest.id != *id || m.state != ModuleState::Active) continue;
            if (services.unregisterTemplates) services.unregisterTemplates(m.manifest.id);
            if (!m.manifest.plugin.isEmpty() && services.stopPlugin) services.stopPlugin(QDir(m.dir).filePath(m.manifest.plugin));
            m.state = ModuleState::Ready;
        }
    m_activationOrder.clear();
}

std::vector<ConverterSpec> ModuleRegistry::importers() const
{
    std::vector<ConverterSpec> out;
    for (const auto& m : m_modules)
        if (m.state == ModuleState::Active) out.insert(out.end(), m.manifest.importers.begin(), m.manifest.importers.end());
    return out;
}

std::vector<ConverterSpec> ModuleRegistry::exporters() const
{
    std::vector<ConverterSpec> out;
    for (const auto& m : m_modules)
        if (m.state == ModuleState::Active) out.insert(out.end(), m.manifest.exporters.begin(), m.manifest.exporters.end());
    return out;
}

bool ModuleRegistry::runConverter(const ConverterSpec& c, const QString& input, const QString& output, QString* log, int timeoutMs) const
{
    const auto* m = module(c.moduleId);
    if (!m || m->state != ModuleState::Active)
    {
        if (log) *log = QStringLiteral("module « %1 » inactif : convertisseur non exécuté").arg(c.moduleId);
        return false;
    }
    QStringList args;
    for (QString a : c.arguments) args << a.replace(QStringLiteral("{input}"), QDir::toNativeSeparators(input)).replace(QStringLiteral("{output}"), QDir::toNativeSeparators(output));
    QProcess p;
    p.setWorkingDirectory(c.moduleDir);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(QDir(c.moduleDir).filePath(c.command), args);
    const bool started = p.waitForStarted(10000);
    const bool finished = started && p.waitForFinished(timeoutMs);
    if (!finished) p.kill();
    if (log)
        *log = QString::fromLocal8Bit(p.readAll()) + (started ? (finished ? QString() : QStringLiteral("\ndélai dépassé : processus arrêté"))
                                                              : QStringLiteral("démarrage impossible : %1").arg(p.errorString()));
    return finished && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0 && QFileInfo::exists(output);
}

} // namespace TSA::Modules
