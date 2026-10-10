// tsa-template : outil des auteurs de templates (docs/TEMPLATES.md §8). Qt Core seul ; mêmes règles que
// l'application (TemplatePackage::validate, moteur TemplateEngine).
//
//   tsa-template validate <paquet.tsatemplate | dossier>
//   tsa-template pack     <dossier> <sortie.tsatemplate>
//   tsa-template render   <paquet | dossier> <rapport> <données.json> <sortie.html> [options.json]
//
// Code de sortie : 0 succès, 1 paquet invalide ou rendu en erreur, 2 usage.

#include "Templates/TemplatePackage.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace TSA::Templates;

namespace
{
bool load(const QString& path, TemplatePackage* p)
{
    QStringList errors;
    const bool ok = QFileInfo(path).isDir() ? TemplatePackage::loadDirectory(path, p, &errors) : TemplatePackage::loadFile(path, p, &errors);
    if (!ok)
    {
        std::fprintf(stderr, "%s : INVALIDE (%lld erreur(s))\n", qUtf8Printable(path), static_cast<long long>(errors.size()));
        for (const auto& e : errors) std::fprintf(stderr, "  %s\n", qUtf8Printable(e));
    }
    return ok;
}

QJsonObject readJson(const QString& path, bool* ok)
{
    QFile f(path);
    QJsonParseError pe;
    const QJsonDocument doc = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll(), &pe) : QJsonDocument();
    *ok = doc.isObject();
    if (!*ok) std::fprintf(stderr, "JSON illisible : %s\n", qUtf8Printable(path));
    return doc.object();
}

int usage()
{
    std::fprintf(stderr, "Usage :\n  tsa-template validate <paquet|dossier>\n  tsa-template pack <dossier> <sortie.tsatemplate>\n"
                         "  tsa-template render <paquet|dossier> <rapport> <données.json> <sortie.html> [options.json]\n");
    return 2;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // messages accentués lisibles dans la console
#endif
    const QStringList a = app.arguments().mid(1);
    if (a.size() < 2) return usage();
    TemplatePackage p;
    if (a[0] == "validate" && a.size() == 2)
    {
        if (!load(a[1], &p)) return 1;
        const auto& m = p.manifest();
        std::printf("%s : VALIDE — %s %s (%s), %lld rapport(s), %lld fichier(s), empreinte %s\n", qUtf8Printable(a[1]), qUtf8Printable(m.id), qUtf8Printable(m.version),
                    qUtf8Printable(m.dataSchema), static_cast<long long>(m.reports.size()), static_cast<long long>(p.files().size()), qUtf8Printable(p.contentHash().left(16)));
        return 0;
    }
    if (a[0] == "pack" && a.size() == 3)
    {
        if (!load(a[1], &p)) return 1;
        QString error;
        if (!p.saveFile(a[2], &error))
        {
            std::fprintf(stderr, "%s\n", qUtf8Printable(error));
            return 1;
        }
        std::printf("%s écrit\n", qUtf8Printable(a[2]));
        return 0;
    }
    if (a[0] == "render" && (a.size() == 5 || a.size() == 6))
    {
        if (!load(a[1], &p)) return 1;
        bool ok = true;
        const QJsonObject data = readJson(a[3], &ok);
        const QJsonObject options = a.size() == 6 ? readJson(a[5], &ok) : QJsonObject();
        if (!ok) return 1;
        const auto r = p.render(a[2], data, options);
        for (const auto& w : r.warnings) std::fprintf(stderr, "avertissement : %s\n", qUtf8Printable(w));
        for (const auto& v : r.missingVariables) std::fprintf(stderr, "donnée absente : %s\n", qUtf8Printable(v));
        for (const auto& e : r.errors) std::fprintf(stderr, "ERREUR : %s\n", qUtf8Printable(e));
        if (!r.ok()) return 1;
        QFile out(a[4]);
        if (!out.open(QIODevice::WriteOnly) || out.write(r.output.toUtf8()) < 0) return 1;
        std::printf("%s écrit (%lld caractères)\n", qUtf8Printable(a[4]), static_cast<long long>(r.output.size()));
        return 0;
    }
    return usage();
}
