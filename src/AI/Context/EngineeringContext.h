#pragma once

// Contexte d'ingénierie structuré transmis au modèle de langage : données réelles du modèle TSA,
// avec unités explicites et provenance. Lecture seule. Les listes sont plafonnées : le détail
// d'un objet s'obtient par les outils (AIToolRegistry), pas en envoyant tout le modèle.

#include "../Checker/StructuralChecker.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <vector>

namespace TSA::Model { class Model; }
namespace TSA::Analysis { class ResultsModel; }

namespace TSA::AI
{

struct ContextOptions
{
    int maxListedMembers = 40;
    int maxListedNodes = 40;
    int maxListedLoads = 40;
    bool includeCheckReport = false;
};

struct EngineeringSources
{
    const TSA::Model::Model* model = nullptr;
    const TSA::Analysis::ResultsModel* results = nullptr;
    bool resultsUpToDate = true;
    QString projectName;
    std::vector<ObjectRef> selection;
};

class EngineeringContextBuilder
{
public:
    /// Vue d'ensemble : projet, unités, effectifs, matériaux, sections, appuis, charges, résultats.
    static QJsonObject summary(const EngineeringSources& src, const ContextOptions& options = {});

    // Blocs détaillés (également servis par les outils)
    static QJsonObject projectInfo(const EngineeringSources& src);
    static QJsonObject units();
    static QJsonArray materials(const TSA::Model::Model& model);
    static QJsonArray sections(const TSA::Model::Model& model);
    static QJsonArray supports(const TSA::Model::Model& model, int limit);
    static QJsonArray loadCases(const TSA::Model::Model& model);
    static QJsonArray loadCombinations(const TSA::Model::Model& model);
    static QJsonArray loads(const TSA::Model::Model& model, int loadCaseId, int limit);
    static QJsonArray members(const TSA::Model::Model& model, const QString& typeFilter, int offset, int limit);
    static QJsonArray nodes(const TSA::Model::Model& model, int offset, int limit);
    static QJsonObject resultsSummary(const EngineeringSources& src);

    /// Détail d'un objet (propriétés + charges appliquées + résultats si disponibles). Vide si inconnu.
    static QJsonObject objectDetails(const EngineeringSources& src, const ObjectRef& ref);
    static QJsonObject selectionContext(const EngineeringSources& src);

    static QString typeFromKey(const QString& anyCase); // « beam », « poutre », « B » → « beam »
};

} // namespace TSA::AI
