#include "Fem3DEngine.h"

#include "Fem3DAdapter.h"
#include "../OpenSees/OpenSeesEngine.h"

#include "Fem3D/Analysis/LinearStaticAnalysis.h"
#include "Fem3D/Core/Fem3DError.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Analysis
{

namespace
{
QString tr(const char* text) { return QCoreApplication::translate("TSA::Analysis::Fem3DEngine", text); }

std::string joined(const std::vector<std::string>& items, std::size_t shown = 6)
{
    std::string s;
    for (std::size_t i = 0; i < items.size() && i < shown; ++i) s += (i ? " ; " : "") + items[i];
    if (items.size() > shown) s += tr(" ; et %1 autre(s)").arg(items.size() - shown).toStdString();
    return s;
}

std::string scientific(double v)
{
    std::ostringstream o;
    o << std::scientific << std::setprecision(2) << v;
    return o.str();
}
} // namespace

Fem3DEngine::Fem3DEngine() : m_openSees(std::make_unique<OpenSeesEngine>()) {}
Fem3DEngine::~Fem3DEngine() = default;

Fem3DEngine::Settings Fem3DEngine::settingsFromContext(const AnalysisContext& context)
{
    const QJsonObject s = context.settingsFor(kId);
    Settings out;
    out.mode = s.value("mode").toString() == QLatin1String("compare") ? Mode::Compare : Mode::Fem3D;
    out.fallback = s.value("fallback").toString() == QLatin1String("opensees") ? Fallback::OpenSees : Fallback::None;
    return out;
}

QJsonObject Fem3DEngine::settingsToJson(const Settings& settings)
{
    return QJsonObject { { "mode", settings.mode == Mode::Compare ? "compare" : "fem3d" },
                         { "fallback", settings.fallback == Fallback::OpenSees ? "opensees" : "none" } };
}

EngineInfo Fem3DEngine::info() const
{
    EngineInfo i;
    i.id = kId;
    i.name = "Fem3D";
    const std::string v = Fem3D::kEngineVersion;              // « TSA Fem3D 0.3.0 »
    i.version = v.substr(v.find_last_of(' ') + 1);
    i.description = tr("Moteur éléments finis 3D de TSA (poutres d'Euler–Bernoulli exactes, rotules, appuis élastiques), "
                       "validé contre OpenSees sur son périmètre. Moteur secondaire : OpenSees reste le moteur par défaut.")
                        .toStdString();
    return i;
}

AnalysisCapabilities Fem3DEngine::capabilities() const
{
    // Strictement le périmètre qualifié (3DDesplacement/docs/validation/QUALIFICATION.md) ; le reste est refusé.
    AnalysisCapabilities c;
    c.supports3D = true;
    c.supportsFrame = true;
    c.supportsSprings = true;
    c.planarElementPolicy = UnsupportedElementPolicy::ExcludeWithWarning;   // comme OpenSees : dalles non maillées
    c.supportsStatic = true;
    c.providesDisplacements = true;
    c.providesReactions = true;
    c.providesElementForces = true;
    c.supportsCustomOptions = true;
    return c;
}

EngineAvailability Fem3DEngine::availability() const
{
    EngineAvailability a;
    a.available = true;   // bibliothèque compilée dans TSA (TSA_HAS_FEM3D)
    return a;
}

ValidationResult Fem3DEngine::validate(const AnalysisContext& context, const AnalysisModel& model) const
{
    ValidationResult v;
    const std::string cat = "Fem3D";
    const Settings s = settingsFromContext(context);
    const Fem3DIntegration::Conversion c = Fem3DIntegration::convert(context, model);
    for (const auto& e : c.errors) v.addError(cat, e);
    for (const auto& n : c.notes) v.addInfo(cat, n);

    const bool openSeesNeeded = (!c.outOfScope.empty() && s.fallback == Fallback::OpenSees) || (c.outOfScope.empty() && s.mode == Mode::Compare);
    if (!c.outOfScope.empty())
    {
        const std::string reasons = joined(c.outOfScope);
        if (s.fallback == Fallback::OpenSees)
            v.addWarning(cat, tr("Hors du périmètre de Fem3D : %1. Calcul confié à OpenSees (secours choisi dans les options de "
                                 "Fem3D) : les résultats porteront l'identité d'OpenSees.").arg(QString::fromStdString(reasons)).toStdString());
        else
            v.addError(cat, tr("Hors du périmètre de Fem3D : %1. Choisir le moteur OpenSees, ou activer le secours OpenSees dans "
                               "les options de Fem3D.").arg(QString::fromStdString(reasons)).toStdString());
        if (s.mode == Mode::Compare)
            v.addWarning(cat, tr("Comparaison impossible : Fem3D ne peut pas calculer ce modèle.").toStdString());
    }
    if (openSeesNeeded)
    {
        const EngineAvailability os = m_openSees->availability();
        if (!os.available)
            v.addError(cat, tr("OpenSees est requis (%1) mais indisponible : %2")
                                .arg(s.mode == Mode::Compare ? tr("comparaison") : tr("secours"), QString::fromStdString(os.message))
                                .toStdString());
        else
            v.merge(m_openSees->validate(context, model));
    }
    return v;
}

AnalysisRunResult Fem3DEngine::run(const AnalysisContext& context, const AnalysisModel& model, const AnalysisRunCallbacks& callbacks)
{
    AnalysisRunResult r;
    const Settings s = settingsFromContext(context);
    auto log = [&](const std::string& line) {
        if (callbacks.log) callbacks.log(line);
    };
    if (callbacks.progress) callbacks.progress(5, tr("Conversion du modèle pour Fem3D…").toStdString());
    const Fem3DIntegration::Conversion c = Fem3DIntegration::convert(context, model);
    if (!c.errors.empty())
    {
        r.message = tr("Fem3D : données refusées : %1").arg(QString::fromStdString(joined(c.errors))).toStdString();
        return r;
    }
    if (!c.outOfScope.empty())
    {
        const std::string reasons = joined(c.outOfScope);
        if (s.fallback != Fallback::OpenSees)
        {
            r.message = tr("Hors du périmètre de Fem3D : %1. Aucun calcul n'a été fait.").arg(QString::fromStdString(reasons)).toStdString();
            return r;
        }
        // Secours EXPLICITE : raisons consignées, résultats étiquetés OpenSees.
        const std::string why = tr("Calcul fait par OpenSees (secours de Fem3D) — hors du périmètre de Fem3D : %1").arg(QString::fromStdString(reasons)).toStdString();
        log("[Fem3D] " + why);
        r = m_openSees->run(context, model, callbacks);
        r.producedBy = m_openSees->info();
        if (r.success)
        {
            auto meta = r.results.executionMetadata();
            meta.warnings.push_back(why);
            r.results.setExecutionMetadata(meta);
            r.results.appendLog("\n[AVERTISSEMENT] " + why + "\n");
            r.message = tr("Calcul OpenSees terminé (secours de Fem3D).").toStdString();
        }
        return r;
    }
    if (callbacks.cancelRequested && callbacks.cancelRequested())
    {
        r.message = tr("Calcul annulé.").toStdString();
        return r;
    }

    if (callbacks.progress) callbacks.progress(20, tr("Assemblage et résolution Fem3D…").toStdString());
    Fem3D::LoadCaseResults fr;
    try
    {
        const Fem3D::LinearStaticAnalysis analysis(c.model);
        fr = analysis.run(1);
    }
    catch (const Fem3D::Fem3DError& error)
    {
        r.message = tr("Fem3D : %1").arg(QString::fromUtf8(error.what())).toStdString();
        if (error.code() == Fem3D::ErrorCode::Mechanism)
            r.message += tr(" Aucun moteur de secours n'est utilisé : un mécanisme est une propriété du modèle (appuis, "
                            "rotules), pas une panne du moteur.").toStdString();
        log("[ERREUR] " + r.message);
        return r;
    }
    if (callbacks.progress) callbacks.progress(70, tr("Remappage des résultats Fem3D…").toStdString());
    r.results = Fem3DIntegration::mapResults(c, model, fr);
    r.success = r.results.isValid();
    r.message = tr("Calcul Fem3D terminé.").toStdString();

    if (s.mode == Mode::Compare)
    {
        if (callbacks.progress) callbacks.progress(80, tr("Calcul de comparaison OpenSees…").toStdString());
        AnalysisRunCallbacks quiet;
        quiet.log = callbacks.log;
        quiet.cancelRequested = callbacks.cancelRequested;
        const AnalysisRunResult os = m_openSees->run(context, model, quiet);
        auto meta = r.results.executionMetadata();
        if (!os.success)
        {
            const std::string w = tr("Comparaison Fem3D / OpenSees impossible : %1").arg(QString::fromStdString(os.message)).toStdString();
            meta.warnings.push_back(w);
            r.results.setExecutionMetadata(meta);
            r.results.appendLog("\n[AVERTISSEMENT] " + w + "\n");
        }
        else
        {
            const EngineComparison cmp = compareResults(r.results, os.results);
            r.message += " " + tr("Comparaison OpenSees : %1 valeurs, %2 hors tolérance.").arg(cmp.compared).arg(cmp.failed).toStdString();
        }
    }
    if (callbacks.progress) callbacks.progress(100, r.message);
    return r;
}

void Fem3DEngine::cancel()
{
    m_openSees->cancel();
}

EngineComparison compareResults(ResultsModel& fem3d, const ResultsModel& openSees)
{
    // Forces et moments comparés en kN, kN·m : Fem3D calcule toujours en kN, OpenSees en kN ou en N selon le projet.
    const double kF = fem3d.units().forceToKiloNewtons(), kO = openSees.units().forceToKiloNewtons();
    // Échelles par nature de grandeur, planchers dimensionnels (comme les benchmarks du moteur) : une grandeur toute
    // nulle n'a pas d'échelle nulle.
    double sT = 0.0, sR = 0.0, sF = 0.0, sM = 0.0, maxLength = 0.0;
    auto grow = [](double& s, double v) {
        if (std::isfinite(v)) s = std::max(s, std::abs(v));
    };
    for (const auto& [id, d] : openSees.allDisplacements())
    {
        for (double v : { d.ux, d.uy, d.uz }) grow(sT, v);
        for (double v : { d.rx, d.ry, d.rz }) grow(sR, v);
    }
    for (const auto& [id, r] : openSees.allReactions())
    {
        for (double v : { r.rx, r.ry, r.rz }) grow(sF, v * kO);
        for (double v : { r.mx, r.my, r.mz }) grow(sM, v * kO);
    }
    for (const auto& [key, e] : openSees.allElementResults())
    {
        maxLength = std::max(maxLength, e.length);
        for (const StationForces* f : { &e.startForces, &e.endForces })
        {
            for (double v : { f->N, f->Vy, f->Vz }) grow(sF, v * kO);
            for (double v : { f->Mx, f->My, f->Mz }) grow(sM, v * kO);
        }
    }
    maxLength = std::max(maxLength, 1e-9);
    sT = std::max(sT, 1e-30);
    sF = std::max(sF, 1e-30);
    sR = std::max(sR, sT / maxLength);
    sM = std::max(sM, sF * maxLength);

    struct Group
    {
        std::string name;
        EngineComparison c;
    };
    std::array<Group, 3> groups { Group { QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Déplacements et rotations").toStdString(), {} },
                                  Group { QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Réactions (appuis et ressorts)").toStdString(), {} },
                                  Group { QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Efforts aux extrémités des barres").toStdString(), {} } };
    auto check = [&](EngineComparison& c, double a, double b, double scale) {
        const double diff = std::abs(a - b), absTol = Fem3DEngine::kCompareAbsoluteFactor * scale;
        ++c.compared;
        if (!(std::isfinite(a) && std::isfinite(b) && diff <= std::max(Fem3DEngine::kCompareRelative * std::abs(b), absTol))) ++c.failed;
        if (std::isfinite(diff)) c.maxAbsolute = std::max(c.maxAbsolute, diff);
        if (std::abs(b) * Fem3DEngine::kCompareRelative > absTol && std::isfinite(diff)) c.maxRelative = std::max(c.maxRelative, diff / std::abs(b));
    };
    for (const auto& [id, d] : fem3d.allDisplacements())
    {
        const NodeDisplacement* o = openSees.getNodeDisplacement(id);
        if (!o) { ++groups[0].c.compared; ++groups[0].c.failed; continue; }
        check(groups[0].c, d.ux, o->ux, sT); check(groups[0].c, d.uy, o->uy, sT); check(groups[0].c, d.uz, o->uz, sT);
        check(groups[0].c, d.rx, o->rx, sR); check(groups[0].c, d.ry, o->ry, sR); check(groups[0].c, d.rz, o->rz, sR);
    }
    for (const auto& [id, r] : fem3d.allReactions())
    {
        const NodeReaction* o = openSees.getNodeReaction(id);
        if (!o) { ++groups[1].c.compared; ++groups[1].c.failed; continue; }
        check(groups[1].c, r.rx * kF, o->rx * kO, sF); check(groups[1].c, r.ry * kF, o->ry * kO, sF); check(groups[1].c, r.rz * kF, o->rz * kO, sF);
        check(groups[1].c, r.mx * kF, o->mx * kO, sM); check(groups[1].c, r.my * kF, o->my * kO, sM); check(groups[1].c, r.mz * kF, o->mz * kO, sM);
    }
    for (const auto& [key, e] : fem3d.allElementResults())
    {
        const ElementResults* o = openSees.getElementResults(key);
        if (!o) { ++groups[2].c.compared; ++groups[2].c.failed; continue; }
        const std::pair<const StationForces*, const StationForces*> ends[2] = { { &e.startForces, &o->startForces }, { &e.endForces, &o->endForces } };
        for (const auto& [a, b] : ends)
        {
            check(groups[2].c, a->N * kF, b->N * kO, sF); check(groups[2].c, a->Vy * kF, b->Vy * kO, sF); check(groups[2].c, a->Vz * kF, b->Vz * kO, sF);
            check(groups[2].c, a->Mx * kF, b->Mx * kO, sM); check(groups[2].c, a->My * kF, b->My * kO, sM); check(groups[2].c, a->Mz * kF, b->Mz * kO, sM);
        }
    }

    EngineComparison total;
    EngineResultTable table;
    table.engineId = Fem3DEngine::kId;
    table.title = QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Comparaison Fem3D / OpenSees (résultats affichés : Fem3D)").toStdString();
    table.columns = { QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Grandeur").toStdString(),
                      QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Valeurs comparées").toStdString(),
                      QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Hors tolérance").toStdString(),
                      QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Écart absolu max").toStdString(),
                      QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Écart relatif max").toStdString() };
    for (const Group& g : groups)
    {
        table.rows.push_back({ g.name, std::to_string(g.c.compared), std::to_string(g.c.failed), scientific(g.c.maxAbsolute), scientific(g.c.maxRelative) });
        total.compared += g.c.compared;
        total.failed += g.c.failed;
        total.maxAbsolute = std::max(total.maxAbsolute, g.c.maxAbsolute);
        total.maxRelative = std::max(total.maxRelative, g.c.maxRelative);
    }
    table.rows.push_back({ QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Total (tolérance : max(1e-6·|réf|, 1e-9·échelle))").toStdString(),
                           std::to_string(total.compared), std::to_string(total.failed), scientific(total.maxAbsolute), scientific(total.maxRelative) });
    fem3d.addEngineTable(table);

    const std::string summary = QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "Comparaison Fem3D / OpenSees : %1 valeurs, %2 hors tolérance (écart relatif max %3).")
                                    .arg(total.compared).arg(total.failed).arg(QString::fromStdString(scientific(total.maxRelative))).toStdString();
    fem3d.appendLog("\n[Fem3D] " + summary + "\n");
    if (total.failed > 0)
    {
        auto meta = fem3d.executionMetadata();
        meta.warnings.push_back(QCoreApplication::translate("TSA::Analysis::Fem3DEngine", "ÉCART entre Fem3D et OpenSees : %1 Les résultats affichés sont ceux de Fem3D ; "
                                                                                          "examiner la table de comparaison.")
                                    .arg(QString::fromStdString(summary)).toStdString());
        fem3d.setExecutionMetadata(meta);
    }
    return total;
}

} // namespace TSA::Analysis
