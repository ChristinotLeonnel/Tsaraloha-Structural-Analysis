#include "HelpTopics.h"

#include <ProductIdentity.h>

#include <QRegularExpression>
#include <QUrlQuery>

#include <array>

namespace TSA::Help
{

namespace
{

// Registre des identifiants d'aide. Format lu par scripts/check-docs.mjs (Tsaraloha-Web) :
// une entrée HelpTopic par ligne (identifiant entre guillemets, puis chemin « docs/… »). Ajouter une entrée = ajouter aussi
// l'identifiant dans l'en-tête « helpIds » de la page de documentation correspondante.
constexpr std::array kTopics{
    // clang-format off
    HelpTopic{ "general.overview",          "docs" },
    HelpTopic{ "general.about",             "docs/getting-started/introduction" },
    HelpTopic{ "project.create",            "docs/getting-started/create-project" },
    HelpTopic{ "project.files",             "docs/getting-started/files" },
    HelpTopic{ "project.topology",          "docs/modeling/numbering" },
    HelpTopic{ "ui.overview",               "docs/getting-started/interface" },
    HelpTopic{ "ui.shortcuts",              "docs/getting-started/interface" },
    HelpTopic{ "model.nodes",               "docs/modeling/nodes" },
    HelpTopic{ "model.members",             "docs/modeling/members" },
    HelpTopic{ "model.cables",              "docs/modeling/cables" },
    HelpTopic{ "model.surfaces",            "docs/modeling/surfaces" },
    HelpTopic{ "model.sections",            "docs/modeling/sections-materials" },
    HelpTopic{ "model.supports",            "docs/modeling/supports" },
    HelpTopic{ "model.grids",               "docs/modeling/grids-workplanes" },
    HelpTopic{ "model.selection",           "docs/modeling/selection" },
    HelpTopic{ "model.snapping",            "docs/modeling/snapping" },
    HelpTopic{ "model.edit",                "docs/modeling/editing-tools" },
    HelpTopic{ "model.units",               "docs/modeling/units-coordinates" },
    HelpTopic{ "loading.overview",          "docs/loading/overview" },
    HelpTopic{ "loading.nodal",             "docs/loading/nodal-loads" },
    HelpTopic{ "loading.member-point",      "docs/loading/point-loads-on-members" },
    HelpTopic{ "loading.distributed",       "docs/loading/distributed-loads" },
    HelpTopic{ "loading.trapezoidal",       "docs/loading/trapezoidal-loads" },
    HelpTopic{ "loading.surface",           "docs/loading/surface-loads" },
    HelpTopic{ "loading.cases",             "docs/loading/load-cases-combinations" },
    HelpTopic{ "loading.selfweight",        "docs/loading/self-weight" },
    HelpTopic{ "mesh.overview",             "docs/mesh/overview" },
    HelpTopic{ "analysis.overview",         "docs/analysis/overview" },
    HelpTopic{ "analysis.engines",          "docs/analysis/engines" },
    HelpTopic{ "results.overview",          "docs/results/overview" },
    HelpTopic{ "results.report",            "docs/results/calculation-report" },
    HelpTopic{ "bim.ifc",                   "docs/bim/ifc" },
    HelpTopic{ "troubleshooting.overview",  "docs/troubleshooting" },
    HelpTopic{ "troubleshooting.report",    "docs/troubleshooting/reporting-bugs" },
    // clang-format on
};

const HelpTopic* findTopic(const QString& topicId)
{
    for (const HelpTopic& t : kTopics)
        if (topicId == QLatin1String(t.id)) return &t;
    return nullptr;
}

/// Chemin de la base sans barre oblique finale (« /Tsaraloha-Web », ou vide à la racine).
QString basePath(const QUrl& baseUrl)
{
    QString p = baseUrl.path();
    while (p.endsWith(QLatin1Char('/'))) p.chop(1);
    return p;
}

} // namespace

std::span<const HelpTopic> topics()
{
    return kTopics;
}

QString pathForTopic(const QString& topicId)
{
    const HelpTopic* t = findTopic(topicId);
    return t ? QString::fromLatin1(t->path) : QString();
}

bool isKnownTopic(const QString& topicId)
{
    return findTopic(topicId) != nullptr;
}

bool isValidBaseUrl(const QUrl& baseUrl)
{
    return baseUrl.isValid() && baseUrl.scheme() == QLatin1String("https") && !baseUrl.host().isEmpty()
        && baseUrl.userInfo().isEmpty() && !baseUrl.hasQuery() && !baseUrl.hasFragment();
}

bool isAllowedHelpUrl(const QUrl& url, const QUrl& baseUrl)
{
    if (!isValidBaseUrl(baseUrl) || !url.isValid()) return false;
    if (url.scheme() != QLatin1String("https")) return false;
    if (url.host().compare(baseUrl.host(), Qt::CaseInsensitive) != 0) return false;
    if (url.port(443) != baseUrl.port(443)) return false;
    if (!url.userInfo().isEmpty() || url.hasFragment()) return false;

    const QString root = basePath(baseUrl);
    const QString path = url.path();
    if (path != root && !path.startsWith(root + QLatin1Char('/'))) return false;
    if (path.contains(QLatin1String("/../")) || path.endsWith(QLatin1String("/.."))) return false;

    for (const auto& [key, value] : QUrlQuery(url).queryItems())
    {
        Q_UNUSED(value);
        if (key != QLatin1String("lang") && key != QLatin1String("v")) return false;
    }
    return true;
}

HelpUrl buildHelpUrl(const QString& baseUrl, const QString& topicId, const QString& language, const QString& version)
{
    HelpUrl result;
    const QString trimmed = baseUrl.trimmed();
    if (trimmed.isEmpty())
    {
        result.error = QStringLiteral("Aucune documentation en ligne n'est configurée pour ce logiciel.");
        return result;
    }
    const QUrl base(trimmed, QUrl::StrictMode);
    if (!isValidBaseUrl(base))
    {
        result.error = QStringLiteral("Adresse de documentation refusée (HTTPS obligatoire) : %1").arg(trimmed);
        return result;
    }

    QString page = pathForTopic(topicId);
    if (page.isEmpty())
    {
        result.knownTopic = false;
        page = pathForTopic(QString::fromLatin1(kOverviewTopic));
    }

    QUrl url(base);
    url.setPath(basePath(base) + QLatin1Char('/') + page);

    QUrlQuery query;
    if (language == QLatin1String("fr") || language == QLatin1String("en"))
        query.addQueryItem(QStringLiteral("lang"), language);
    static const QRegularExpression semver(QStringLiteral("^\\d{1,4}\\.\\d{1,4}\\.\\d{1,4}$"));
    if (semver.match(version).hasMatch()) query.addQueryItem(QStringLiteral("v"), version);
    if (!query.isEmpty()) url.setQuery(query);

    if (!isAllowedHelpUrl(url, base))
    {
        result.error = QStringLiteral("Adresse d'aide refusée : %1").arg(url.toString());
        return result;
    }
    result.url = url;
    return result;
}

HelpUrl officialHelpUrl(const QString& topicId, const QString& language)
{
    return buildHelpUrl(QString::fromUtf8(TSA::Product::kDocsBaseUrl), topicId, language,
                        QString::fromUtf8(TSA::Product::kVersion));
}

} // namespace TSA::Help
