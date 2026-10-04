#pragma once

// Agent de vérification déterministe du modèle TSA. Il ne dépend d'aucun modèle de langage :
// ses constats sont des FAITS calculés sur le modèle (ou des AVERTISSEMENTS fondés sur une
// hypothèse explicitement nommée). Le LLM ne fait que les expliquer et les hiérarchiser.

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <vector>

namespace TSA::Model { class Model; }
namespace TSA::Analysis { class ResultsModel; }

namespace TSA::AI
{

enum class FindingSeverity { Info, Warning, Error };

struct ObjectRef
{
    QString type; // node, beam, column, slab, wall, foundation, truss, cable, loadCase, combination
    int id = 0;
    QString label() const;
};

struct CheckFinding
{
    FindingSeverity severity = FindingSeverity::Info;
    QString code;      // identifiant stable, ex. « TOPO_ISOLATED_NODE »
    QString category;  // Topologie, Stabilité, Propriétés, Charges, Résultats, Calcul
    QString message;
    QString basis;     // « FAIT » ou « HYPOTHÈSE : … » (base du constat)
    std::vector<ObjectRef> objects;
    QJsonObject toJson() const;
};

struct CheckReport
{
    std::vector<CheckFinding> findings;
    int errors() const;
    int warnings() const;
    QJsonObject toJson() const;
    QString toText() const; // résumé lisible (sans LLM)
};

struct CheckOptions
{
    double coincidentTolerance = 1e-3;        // m
    double displacementSpanRatio = 250.0;     // hypothèse de contrôle : L/250
    bool resultsUpToDate = true;
    int maxObjectsPerFinding = 25;
};

class StructuralChecker
{
public:
    static CheckReport check(const TSA::Model::Model& model, const TSA::Analysis::ResultsModel* results = nullptr,
                             const CheckOptions& options = {});
};

QString severityLabel(FindingSeverity s);

} // namespace TSA::AI
