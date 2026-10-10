#include "TemplatePackage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>

#include <set>

namespace TSA::Templates
{

namespace
{
constexpr qsizetype kMaxPackageBytes = 32 * 1024 * 1024;
constexpr int kMaxFiles = 500;

const QStringList& textExtensions()
{
    static const QStringList e { "html", "htm", "css", "txt", "md", "svg", "json" };
    return e;
}
const QStringList& binaryExtensions()
{
    static const QStringList e { "png", "jpg", "jpeg", "gif" };
    return e;
}
QString extensionOf(const QString& name) { return QFileInfo(name).suffix().toLower(); }

QString mimeOf(const QString& name)
{
    const QString e = extensionOf(name);
    if (e == "png") return QStringLiteral("image/png");
    if (e == "jpg" || e == "jpeg") return QStringLiteral("image/jpeg");
    if (e == "gif") return QStringLiteral("image/gif");
    if (e == "svg") return QStringLiteral("image/svg+xml");
    return {};
}

/// Constructions interdites dans les fichiers texte (exécution de code, dépendances externes).
struct Rule
{
    QRegularExpression re;
    const char* why;
};
const std::vector<Rule>& securityRules()
{
    static const auto ci = QRegularExpression::CaseInsensitiveOption;
    static const std::vector<Rule> rules {
        { QRegularExpression(QStringLiteral("<\\s*script"), ci), "balise <script>" },
        { QRegularExpression(QStringLiteral("<\\s*(iframe|frame|object|embed|applet|form|base|link)\\b"), ci), "élément embarqué ou externe" },
        { QRegularExpression(QStringLiteral("<\\s*meta[^>]*http-equiv"), ci), "<meta http-equiv>" },
        { QRegularExpression(QStringLiteral("\\son[a-z]+\\s*="), ci), "gestionnaire d'événement (on…=)" },
        { QRegularExpression(QStringLiteral("(java|vb)script\\s*:"), ci), "URL javascript:" },
        { QRegularExpression(QStringLiteral("expression\\s*\\("), ci), "expression CSS" },
        { QRegularExpression(QStringLiteral("@import"), ci), "@import" },
        { QRegularExpression(QStringLiteral("url\\(\\s*['\"]?\\s*(https?:|ftp:|file:|//)"), ci), "ressource distante (url())" },
        { QRegularExpression(QStringLiteral("\\bsrc\\s*=\\s*['\"]?\\s*(https?:|ftp:|file:|//)"), ci), "ressource distante (src=)" },
    };
    return rules;
}

bool safePath(const QString& name)
{
    return !name.isEmpty() && !name.startsWith('/') && !name.contains('\\') && !name.contains(':') && !name.split('/').contains(QStringLiteral(".."))
        && !name.split('/').contains(QStringLiteral("")) && !name.split('/').contains(QStringLiteral("."));
}

QString assetKey(const QString& name)
{
    QString k = name;
    for (QChar& c : k)
        if (!c.isLetterOrNumber()) c = QLatin1Char('_');
    return k;
}

QJsonArray stringArray(const QStringList& l)
{
    QJsonArray a;
    for (const auto& s : l) a.append(s);
    return a;
}

QStringList toStringList(const QJsonValue& v)
{
    QStringList l;
    for (const auto& e : v.toArray()) l << e.toString();
    return l;
}

TemplateManifest manifestFromJson(const QJsonObject& m)
{
    TemplateManifest t;
    t.raw = m;
    t.id = m.value("id").toString();
    t.name = m.value("name").toString();
    t.version = m.value("version").toString();
    t.description = m.value("description").toString();
    t.author = m.value("author").toString();
    t.license = m.value("license").toString();
    t.engine = m.value("engine").toString(QString::fromLatin1(kEngineVersion));
    t.dataSchema = m.value("dataSchema").toString();
    t.applications = toStringList(m.value("applications"));
    t.basedOn = m.value("basedOn").toString();
    t.baseHash = m.value("baseHash").toString();
    for (const auto& v : m.value("reports").toArray())
    {
        const QJsonObject r = v.toObject();
        t.reports.push_back({ r.value("id").toString(), r.value("name").toString(), r.value("description").toString(), r.value("entry").toString(),
                              toStringList(r.value("requires")) });
    }
    for (const auto& v : m.value("options").toArray())
    {
        const QJsonObject o = v.toObject();
        t.options.push_back({ o.value("id").toString(), o.value("label").toString(), o.value("type").toString("text"), o.value("default"),
                              toStringList(o.value("choices")) });
    }
    return t;
}

QJsonObject manifestToJson(const TemplateManifest& t)
{
    QJsonObject m = t.raw;   // champs inconnus conservés
    m["id"] = t.id;
    m["name"] = t.name;
    m["version"] = t.version;
    m["engine"] = t.engine;
    m["dataSchema"] = t.dataSchema;
    auto opt = [&](const char* k, const QString& v) {
        if (v.isEmpty()) m.remove(QLatin1String(k));
        else m[QLatin1String(k)] = v;
    };
    opt("description", t.description);
    opt("author", t.author);
    opt("license", t.license);
    opt("basedOn", t.basedOn);
    opt("baseHash", t.baseHash);
    if (t.applications.isEmpty()) m.remove("applications");
    else m["applications"] = stringArray(t.applications);
    QJsonArray reports;
    for (const auto& r : t.reports)
    {
        QJsonObject o { { "id", r.id }, { "name", r.name }, { "entry", r.entry } };
        if (!r.description.isEmpty()) o["description"] = r.description;
        if (!r.required.isEmpty()) o["requires"] = stringArray(r.required);
        reports.append(o);
    }
    m["reports"] = reports;
    QJsonArray options;
    for (const auto& o : t.options)
    {
        QJsonObject j { { "id", o.id }, { "label", o.label }, { "type", o.type } };
        if (!o.defaultValue.isUndefined()) j["default"] = o.defaultValue;
        if (!o.choices.isEmpty()) j["choices"] = stringArray(o.choices);
        options.append(j);
    }
    if (options.isEmpty()) m.remove("options");
    else m["options"] = options;
    return m;
}
} // namespace

const TemplateReport* TemplateManifest::report(const QString& reportId) const
{
    for (const auto& r : reports)
        if (r.id == reportId) return &r;
    return nullptr;
}

QJsonValue valueAtPath(const QJsonObject& data, const QString& path)
{
    QJsonValue v = data;
    for (const QString& part : path.split('.'))
    {
        if (!v.isObject() || !v.toObject().contains(part)) return QJsonValue(QJsonValue::Undefined);
        v = v.toObject().value(part);
    }
    return v;
}

// --- Lecture / écriture --------------------------------------------------------------------------------

bool TemplatePackage::fromJson(const QJsonObject& json, TemplatePackage* out, QStringList* errors)
{
    QStringList errs;
    TemplatePackage p;
    if (json.value("format").toString() != QLatin1String(kPackageFormat))
        errs << QStringLiteral("format : « %1 » attendu (lu « %2 »)").arg(QLatin1String(kPackageFormat), json.value("format").toString());
    p.m_manifest = manifestFromJson(json.value("manifest").toObject());
    const QJsonObject files = json.value("files").toObject();
    for (auto it = files.begin(); it != files.end(); ++it)
    {
        const QJsonObject f = it.value().toObject();
        if (f.contains("text")) p.m_files.insert(it.key(), f.value("text").toString().toUtf8());
        else if (f.contains("base64"))
        {
            const auto decoded = QByteArray::fromBase64Encoding(f.value("base64").toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
            if (!decoded) errs << QStringLiteral("files.%1 : base64 invalide").arg(it.key());
            else p.m_files.insert(it.key(), *decoded);
        }
        else
            errs << QStringLiteral("files.%1 : « text » ou « base64 » attendu").arg(it.key());
    }
    if (errs.isEmpty()) errs = p.validate();
    if (errors) *errors = errs;
    if (out && errs.isEmpty()) *out = p;
    return errs.isEmpty();
}

bool TemplatePackage::loadFile(const QString& path, TemplatePackage* out, QStringList* errors)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (errors) *errors = { QStringLiteral("lecture impossible : %1").arg(path) };
        return false;
    }
    if (f.size() > kMaxPackageBytes * 2)
    {
        if (errors) *errors = { QStringLiteral("fichier trop volumineux (%1 octets)").arg(f.size()) };
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
    {
        if (errors) *errors = { QStringLiteral("JSON invalide : %1 (position %2)").arg(pe.errorString()).arg(pe.offset) };
        return false;
    }
    return fromJson(doc.object(), out, errors);
}

bool TemplatePackage::loadDirectory(const QString& directory, TemplatePackage* out, QStringList* errors)
{
    const QString dir = QFileInfo(directory).absoluteFilePath();   // chemins internes relatifs à la racine du paquet
    QFile mf(dir + QStringLiteral("/manifest.json"));
    if (!mf.open(QIODevice::ReadOnly))
    {
        if (errors) *errors = { QStringLiteral("manifest.json absent de %1").arg(dir) };
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument manifest = QJsonDocument::fromJson(mf.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        if (errors) *errors = { QStringLiteral("manifest.json invalide : %1").arg(pe.errorString()) };
        return false;
    }
    QJsonObject files;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString path = it.next();
        const QString rel = QDir(dir).relativeFilePath(path);
        if (rel == QLatin1String("manifest.json")) continue;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray bytes = f.readAll();
        if (textExtensions().contains(extensionOf(rel))) files.insert(rel, QJsonObject { { "text", QString::fromUtf8(bytes) } });
        else files.insert(rel, QJsonObject { { "base64", QString::fromLatin1(bytes.toBase64()) } });
    }
    return fromJson(QJsonObject { { "format", QLatin1String(kPackageFormat) }, { "manifest", manifest.object() }, { "files", files } }, out, errors);
}

QJsonObject TemplatePackage::toJson() const
{
    QJsonObject files;
    for (auto it = m_files.begin(); it != m_files.end(); ++it)
    {
        if (textExtensions().contains(extensionOf(it.key()))) files.insert(it.key(), QJsonObject { { "text", QString::fromUtf8(it.value()) } });
        else files.insert(it.key(), QJsonObject { { "base64", QString::fromLatin1(it.value().toBase64()) } });
    }
    return QJsonObject { { "format", QLatin1String(kPackageFormat) }, { "manifest", manifestToJson(m_manifest) }, { "files", files } };
}

bool TemplatePackage::saveFile(const QString& path, QString* error) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented)) < 0 || !f.commit())
    {
        if (error) *error = QStringLiteral("écriture impossible de %1 : %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

QString TemplatePackage::contentHash() const
{
    QJsonObject m = manifestToJson(m_manifest);
    m.remove("basedOn");
    m.remove("baseHash");
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(QJsonDocument(m).toJson(QJsonDocument::Compact));
    for (auto it = m_files.begin(); it != m_files.end(); ++it)   // QMap : ordre des noms
    {
        h.addData(it.key().toUtf8());
        h.addData(QByteArrayView("\0", 1));
        h.addData(it.value());
    }
    return QString::fromLatin1(h.result().toHex());
}

// --- Validation ----------------------------------------------------------------------------------------

QStringList TemplatePackage::validate() const
{
    QStringList errs;
    const auto& m = m_manifest;
    static const QRegularExpression idRe(QStringLiteral("^[a-z0-9]+(\\.[a-z0-9_-]+)+$"));
    static const QRegularExpression verRe(QStringLiteral("^\\d+\\.\\d+(\\.\\d+)?$"));
    static const QRegularExpression reportRe(QStringLiteral("^[a-z0-9][a-z0-9_-]*$"));
    if (!idRe.match(m.id).hasMatch()) errs << QStringLiteral("manifest.id : « %1 » invalide (ex. « societe.note-beton »)").arg(m.id);
    if (m.name.isEmpty()) errs << QStringLiteral("manifest.name : obligatoire");
    if (!verRe.match(m.version).hasMatch()) errs << QStringLiteral("manifest.version : « majeur.mineur[.correctif] » attendu");
    if (m.engine.section('/', 0, 0) != QLatin1String("tsa-template")) errs << QStringLiteral("manifest.engine : « %1 » attendu").arg(QLatin1String(kEngineVersion));
    else if (m.engine.section('/', 1).toInt() > 1)
        errs << QStringLiteral("manifest.engine : %1 plus récent que le moteur (%2) : mettre à jour l'application").arg(m.engine, QLatin1String(kEngineVersion));
    if (m.dataSchema != QLatin1String("tsa-report-data/1") && m.dataSchema != QLatin1String("tsa-ndc/1"))
        errs << QStringLiteral("manifest.dataSchema : « tsa-report-data/1 » ou « tsa-ndc/1 » attendu (lu « %1 »)").arg(m.dataSchema);
    for (const QString& a : m.applications)
        if (a != "TSA" && a != "TSALab") errs << QStringLiteral("manifest.applications : « %1 » inconnue").arg(a);
    if (m.reports.empty()) errs << QStringLiteral("manifest.reports : au moins un rapport attendu");
    std::set<QString> ids;
    for (const auto& r : m.reports)
    {
        if (!reportRe.match(r.id).hasMatch()) errs << QStringLiteral("reports : identifiant « %1 » invalide").arg(r.id);
        if (!ids.insert(r.id).second) errs << QStringLiteral("reports : identifiant « %1 » en double").arg(r.id);
        if (r.name.isEmpty()) errs << QStringLiteral("reports.%1.name : obligatoire").arg(r.id);
        if (!m_files.contains(r.entry)) errs << QStringLiteral("reports.%1.entry : fichier « %2 » absent du paquet").arg(r.id, r.entry);
    }
    static const QStringList optionTypes { "text", "color", "bool", "number", "choice", "image" };
    std::set<QString> optIds;
    for (const auto& o : m.options)
    {
        if (!reportRe.match(o.id.toLower()).hasMatch() || !optIds.insert(o.id).second) errs << QStringLiteral("options : identifiant « %1 » invalide ou en double").arg(o.id);
        if (!optionTypes.contains(o.type)) errs << QStringLiteral("options.%1 : type « %2 » inconnu").arg(o.id, o.type);
        if (o.type == "choice" && o.choices.isEmpty()) errs << QStringLiteral("options.%1 : choix attendus").arg(o.id);
    }

    if (m_files.size() > kMaxFiles) errs << QStringLiteral("files : plus de %1 fichiers").arg(kMaxFiles);
    qsizetype total = 0;
    for (auto it = m_files.begin(); it != m_files.end(); ++it)
    {
        const QString& name = it.key();
        total += it.value().size();
        if (!safePath(name))
        {
            errs << QStringLiteral("files : chemin « %1 » refusé (relatif, sans « .. », « \\ » ni « : »)").arg(name);
            continue;
        }
        const QString ext = extensionOf(name);
        if (!textExtensions().contains(ext) && !binaryExtensions().contains(ext))
        {
            errs << QStringLiteral("files : type de fichier « %1 » non autorisé (%2)").arg(ext, name);
            continue;
        }
        if (!textExtensions().contains(ext)) continue;
        const QString text = QString::fromUtf8(it.value());
        for (const auto& rule : securityRules())
        {
            const auto match = rule.re.match(text);
            if (match.hasMatch())
            {
                const int line = int(QStringView(text).left(match.capturedStart()).count('\n')) + 1;
                errs << QStringLiteral("%1, ligne %2 : %3 interdit (un template ne contient ni code ni ressource externe)").arg(name).arg(line).arg(QString::fromUtf8(rule.why));
            }
        }
        if (ext == "html" || ext == "htm" || ext == "txt" || ext == "md" || ext == "css")
        {
            for (const auto& e : TemplateEngine::check(text)) errs << QStringLiteral("%1, %2").arg(name, e);
            for (const auto& p : TemplateEngine::referencedPartials(text))
                if (!resolvePartial(p)) errs << QStringLiteral("%1 : partiel « %2 » absent du paquet").arg(name, p);
        }
    }
    if (total > kMaxPackageBytes) errs << QStringLiteral("paquet trop volumineux (%1 octets, maximum %2)").arg(total).arg(kMaxPackageBytes);
    return errs;
}

// --- Rendu ---------------------------------------------------------------------------------------------

std::optional<QString> TemplatePackage::resolvePartial(const QString& name) const
{
    for (const QString& candidate : { name, QStringLiteral("partials/%1.html").arg(name), name + QStringLiteral(".html") })
        if (m_files.contains(candidate) && safePath(candidate)) return QString::fromUtf8(m_files.value(candidate));
    return std::nullopt;
}

QStringList TemplatePackage::missingRequired(const QString& reportId, const QJsonObject& data) const
{
    QStringList missing;
    if (const auto* r = m_manifest.report(reportId))
        for (const QString& path : r->required)
        {
            const QJsonValue v = valueAtPath(data, path);
            if (v.isUndefined() || v.isNull()) missing << path;
        }
    return missing;
}

QJsonObject TemplatePackage::effectiveOptions(const QJsonObject& values, QStringList* errors) const
{
    static const QRegularExpression colorRe(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    static const QRegularExpression imageRe(QStringLiteral("^data:image/(png|jpeg|gif|svg\\+xml);base64,[A-Za-z0-9+/=]+$"));
    QJsonObject out;
    for (const auto& o : m_manifest.options)
    {
        QJsonValue v = o.defaultValue.isUndefined() ? QJsonValue(QString()) : o.defaultValue;
        if (values.contains(o.id))
        {
            const QJsonValue given = values.value(o.id);
            bool ok = false;
            if (o.type == "bool") ok = given.isBool();
            else if (o.type == "number") ok = given.isDouble();
            else if (o.type == "color") ok = colorRe.match(given.toString()).hasMatch();
            else if (o.type == "choice") ok = o.choices.contains(given.toString());
            else if (o.type == "image") ok = given.toString().isEmpty() || imageRe.match(given.toString()).hasMatch();
            else ok = given.isString();
            if (ok) v = given;
            else if (errors) errors->append(QStringLiteral("option « %1 » : valeur refusée pour le type %2 (valeur par défaut conservée)").arg(o.id, o.type));
        }
        out.insert(o.id, v);
    }
    return out;
}

RenderResult TemplatePackage::render(const QString& reportId, const QJsonObject& data, const QJsonObject& optionValues) const
{
    RenderResult result;
    const auto* report = m_manifest.report(reportId);
    if (!report)
    {
        result.errors << QStringLiteral("rapport « %1 » absent du paquet %2").arg(reportId, m_manifest.id);
        return result;
    }
    const QStringList missing = missingRequired(reportId, data);
    if (!missing.isEmpty())
    {
        result.errors << QStringLiteral("données obligatoires absentes : %1").arg(missing.join(", "));
        return result;
    }
    QJsonObject full = data;
    QStringList optionErrors;
    full["options"] = effectiveOptions(optionValues, &optionErrors);
    QJsonObject assets;
    for (auto it = m_files.begin(); it != m_files.end(); ++it)
        if (!mimeOf(it.key()).isEmpty())
            assets.insert(assetKey(it.key()), QStringLiteral("data:%1;base64,%2").arg(mimeOf(it.key()), QString::fromLatin1(it.value().toBase64())));
    full["assets"] = assets;
    full["template"] = QJsonObject { { "id", m_manifest.id }, { "name", m_manifest.name }, { "version", m_manifest.version }, { "report", reportId }, { "reportName", report->name } };
    result = TemplateEngine::render(fileText(report->entry), full, [this](const QString& n) { return resolvePartial(n); });
    result.warnings = optionErrors + result.warnings;   // option refusée : valeur par défaut, rendu poursuivi
    return result;
}

} // namespace TSA::Templates
