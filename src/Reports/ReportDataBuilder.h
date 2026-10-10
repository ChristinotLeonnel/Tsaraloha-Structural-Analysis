#pragma once

// Modèle de données des rapports « tsa-report-data/1 » (docs/TEMPLATES.md) : ce que les templates de
// rapport reçoivent, construit à partir du modèle et, s'il y en a, des résultats du dernier calcul. Aucune
// mise en page ici ; aucune valeur inventée : chaque bloc de résultats porte « available » et, s'il est
// indisponible, la raison (« reason »). Les nombres sont fournis déjà formatés avec leur unité (les
// templates ne calculent rien).
//
// Couches : moteur de calcul → ResultsModel → ReportDataBuilder (ici) → template → rendu (DocumentRenderer).

#include <QJsonObject>
#include <QString>

namespace TSA::Model
{
class Model;
}
namespace TSA::Analysis
{
class ResultsModel;
}

namespace TSA::Reports
{

inline constexpr char kReportDataSchema[] = "tsa-report-data/1";

struct ReportProjectInfo
{
    QString title, description, number, documentNumber, revision, status, engineer, organization, client, date;
};

class ReportDataBuilder
{
public:
    /// @param results  résultats du dernier calcul (nullptr ou invalides : blocs « available: false »)
    static QJsonObject build(const TSA::Model::Model& model, const TSA::Analysis::ResultsModel* results, const ReportProjectInfo& project);
};

} // namespace TSA::Reports
