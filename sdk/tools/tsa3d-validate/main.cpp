// tsa3d-validate : validateur en ligne de commande des fichiers TSA3D (docs/TSA3D.md).
// Mêmes contrôles que l'import de TSA (Tsa3d::validate) : format, version, unités, identifiants, références,
// géométrie, maillage. Code de sortie : 0 valide, 1 au moins une erreur, 2 usage.
//
//   tsa3d-validate modele.tsa3d [autre.tsa3d ...] [--quiet]

#include "IO/Tsa3d/Tsa3d.h"

#include <QCoreApplication>
#include <QStringList>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // messages accentués lisibles dans la console
#endif
    QStringList files;
    bool quiet = false;
    for (const QString& a : app.arguments().mid(1))
    {
        if (a == "--quiet" || a == "-q") quiet = true;
        else if (a == "--help" || a == "-h")
        {
            std::printf("Usage : tsa3d-validate <fichier.tsa3d>... [--quiet]\nFormat pris en charge : TSA3D %s\n",
                        qUtf8Printable(TSA::IO::Tsa3d::versionString()));
            return 0;
        }
        else files << a;
    }
    if (files.isEmpty())
    {
        std::fprintf(stderr, "Usage : tsa3d-validate <fichier.tsa3d>... [--quiet]\n");
        return 2;
    }
    int failures = 0;
    for (const QString& f : files)
    {
        QJsonObject doc;
        TSA::IO::Tsa3d::Report report;
        if (TSA::IO::Tsa3d::readFile(f, &doc, &report)) report = TSA::IO::Tsa3d::validate(doc);
        const int errors = report.count(TSA::IO::Tsa3d::Severity::Error), warnings = report.count(TSA::IO::Tsa3d::Severity::Warning);
        std::printf("%s : %s (%d erreur(s), %d avertissement(s))\n", qUtf8Printable(f), errors ? "INVALIDE" : "VALIDE", errors, warnings);
        for (const QString& line : report.lines())
            if (!quiet || line.startsWith("ERREUR")) std::printf("  %s\n", qUtf8Printable(line));
        failures += errors ? 1 : 0;
    }
    return failures ? 1 : 0;
}
