// Exemple d'utilisation de la bibliothèque TSA (TSA_Model) par un programme C++/Qt : lecture d'un fichier
// TSA3D, validation, construction du modèle TSA, statistiques, export TSA3D. Aucune interface graphique.
//
//   tsa3d_roundtrip_example <entrée.tsa3d> <sortie.tsa3d>

#include "IO/Tsa3d/Tsa3d.h"
#include "Model/Model.h"

#include <QCoreApplication>

#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc < 3)
    {
        std::fprintf(stderr, "Usage : tsa3d_roundtrip_example <entrée.tsa3d> <sortie.tsa3d>\n");
        return 2;
    }
    QJsonObject doc;
    TSA::IO::Tsa3d::Report report;
    if (!TSA::IO::Tsa3d::readFile(QString::fromLocal8Bit(argv[1]), &doc, &report))
    {
        for (const auto& l : report.lines()) std::fprintf(stderr, "%s\n", qPrintable(l));
        return 1;
    }
    TSA::Model::Model model;
    const auto imported = TSA::IO::Tsa3d::importDocument(doc, model);   // valide d'abord ; modèle inchangé si refus
    for (const auto& l : imported.report.lines()) std::printf("%s\n", qPrintable(l));
    if (!imported.ok) return 1;
    std::printf("Modèle : %zu nœuds, %zu poutres, %zu poteaux, %zu dalles\n", model.nodes().size(), model.beams().size(),
                model.columns().size(), model.slabs().size());

    const auto exported = TSA::IO::Tsa3d::exportModel(model, { QStringLiteral("Aller-retour"), QString(), nullptr, false, false });
    QString err;
    if (!TSA::IO::Tsa3d::writeFile(QString::fromLocal8Bit(argv[2]), exported.document, &err))
    {
        std::fprintf(stderr, "%s\n", qPrintable(err));
        return 1;
    }
    std::printf("%s écrit\n", argv[2]);
    return 0;
}
