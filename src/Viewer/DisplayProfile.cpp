#include "DisplayProfile.h"

#include "AnnotationStyle.h"
#include "OccView.h"
#include "ResultsVisualManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

#include <utility>
#include <vector>

namespace TSA::Viewer
{

namespace
{
template <typename E>
using Names = std::vector<std::pair<E, const char*>>;

const Names<ModelDisplayMode>& modelModes()
{
    static const Names<ModelDisplayMode> n { { ModelDisplayMode::Physical, "physical" }, { ModelDisplayMode::Analytical, "analytical" },
                                             { ModelDisplayMode::FiniteElement, "finite_element" }, { ModelDisplayMode::Overlay, "overlay" } };
    return n;
}
const Names<RenderDisplayMode>& renderModes()
{
    static const Names<RenderDisplayMode> n { { RenderDisplayMode::Materials, "materials" }, { RenderDisplayMode::Structure, "structure" },
                                              { RenderDisplayMode::Analysis, "analysis" }, { RenderDisplayMode::Results, "results" },
                                              { RenderDisplayMode::Wireframe, "wireframe" } };
    return n;
}
const Names<TSA::Geometry::DiagramType>& diagrams()
{
    using D = TSA::Geometry::DiagramType;
    static const Names<D> n { { D::None, "none" },           { D::AxialForceN, "N" },       { D::ShearForceVy, "Vy" },   { D::ShearForceVz, "Vz" },
                              { D::TorsionMx, "Mx" },        { D::BendingMy, "My" },        { D::BendingMz, "Mz" },      { D::DeflectionUx, "Ux" },
                              { D::DeflectionUy, "Uy" },     { D::DeflectionUz, "Uz" },     { D::DeflectionUres, "U" },  { D::RotationRx, "Rx" },
                              { D::RotationRy, "Ry" },       { D::RotationRz, "Rz" },       { D::StressSigma, "sigma" } };
    return n;
}
const Names<ScalePreset>& presets()
{
    static const Names<ScalePreset> n { { ScalePreset::Auto, "auto" },   { ScalePreset::X1, "x1" },         { ScalePreset::X10, "x10" },
                                        { ScalePreset::X100, "x100" },   { ScalePreset::X1000, "x1000" },   { ScalePreset::X10000, "x10000" },
                                        { ScalePreset::Custom, "custom" } };
    return n;
}
const Names<DeformedDisplayMode>& deformedModes()
{
    static const Names<DeformedDisplayMode> n { { DeformedDisplayMode::UndeformedOnly, "undeformed" }, { DeformedDisplayMode::DeformedOnly, "deformed" },
                                                { DeformedDisplayMode::Both, "both" } };
    return n;
}
const Names<TSA::Geometry::LoadScaleMode>& loadModes()
{
    using L = TSA::Geometry::LoadScaleMode;
    static const Names<L> n { { L::Proportional, "proportional" }, { L::Automatic, "automatic" }, { L::Normalized, "normalized" } };
    return n;
}
const Names<OccView::ElementCategory>& categories()
{
    using C = OccView::ElementCategory;
    static const Names<C> n { { C::Beams, "beams" }, { C::Columns, "columns" }, { C::Slabs, "slabs" }, { C::Walls, "walls" },
                              { C::Foundations, "foundations" }, { C::Trusses, "trusses" }, { C::Cables, "cables" } };
    return n;
}

template <typename E>
QString nameOf(const Names<E>& names, E v)
{
    for (const auto& [e, s] : names)
        if (e == v) return QString::fromLatin1(s);
    return {};
}
template <typename E>
bool valueOf(const Names<E>& names, const QJsonValue& j, E* out)
{
    const QString s = j.toString();
    for (const auto& [e, n] : names)
        if (s == QLatin1String(n))
        {
            *out = e;
            return true;
        }
    return false;
}

QString normalizedPath(const QString& path)
{
    const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    return p.toLower();
#else
    return p;
#endif
}
} // namespace

QStringList displayProfileKeys()
{
    return { "modelDisplayMode", "colors",       "nodes",          "nodeLabels",      "supports",         "supportLabels",
             "grid",             "gridLabels",   "gridLevels",     "workPlane",       "categories",       "loads",
             "surfaceLoads",     "forces",       "moments",        "loadValues",      "loadScale",        "loadScaleMode",
             "loadMinRatio",     "loadLegend",   "deformed",       "deformedMode",    "deformationPreset", "deformationScale",
             "diagram",          "diagramMultiplier", "diagramLabels", "reactions",    "legend",           "sectionRotation",
             "annotationMode",   "annotationPixels",  "annotationModelHeight" };
}

QJsonObject captureDisplaySettings(const OccView& view)
{
    QJsonObject o;
    o["modelDisplayMode"] = nameOf(modelModes(), view.modelDisplayMode());
    o["colors"] = nameOf(renderModes(), view.renderDisplayMode());
    o["nodes"] = view.areNodesVisible();
    o["nodeLabels"] = view.areNodeLabelsVisible();
    o["supports"] = view.areSupportsVisible();
    o["supportLabels"] = view.areSupportLabelsVisible();
    o["grid"] = view.isGridVisible();
    o["gridLabels"] = view.areGridLabelsVisible();
    o["gridLevels"] = view.areGridLevelsVisible();
    o["workPlane"] = view.isWorkPlaneVisible();
    QJsonObject cats;
    for (const auto& [c, n] : categories()) cats[QString::fromLatin1(n)] = view.isElementCategoryVisible(c);
    o["categories"] = cats;
    o["loads"] = view.areLoadsVisible();
    o["surfaceLoads"] = view.surfaceLoadsVisible();
    o["forces"] = view.areForcesVisible();
    o["moments"] = view.areMomentsVisible();
    o["loadValues"] = view.areLoadValuesVisible();
    o["loadScale"] = view.loadScale();
    o["loadScaleMode"] = nameOf(loadModes(), view.loadScaleSettings().mode);
    o["loadMinRatio"] = view.loadScaleSettings().minRatio;
    o["loadLegend"] = view.loadScaleSettings().showLegend;
    if (const auto* r = view.resultsVisual())
    {
        o["deformed"] = r->isDeformedVisible();
        o["deformedMode"] = nameOf(deformedModes(), r->deformedDisplayMode());
        o["deformationPreset"] = nameOf(presets(), r->deformationScalePreset());
        if (r->deformationScalePreset() == ScalePreset::Custom) o["deformationScale"] = r->deformationScale();
        o["diagram"] = nameOf(diagrams(), r->diagramType());
        o["diagramMultiplier"] = r->diagramScaleMultiplier();
        o["diagramLabels"] = r->areDiagramLabelsVisible();
        o["reactions"] = r->areReactionsVisible();
        o["legend"] = r->isLegendVisible();
        o["sectionRotation"] = r->isSectionRotationVisible();
    }
    const auto& a = AnnotationStyle::instance().settings();
    o["annotationMode"] = a.mode == AnnotationSizeMode::ScreenConstant ? "screen" : "model";
    o["annotationPixels"] = a.basePixels;
    o["annotationModelHeight"] = a.modelHeight;
    return o;
}

QStringList applyDisplaySettings(OccView& view, const QJsonObject& s)
{
    QStringList unknown;
    const QStringList keys = displayProfileKeys();
    for (auto it = s.begin(); it != s.end(); ++it)
        if (!keys.contains(it.key())) unknown << it.key();
    auto boolean = [&](const char* key, auto&& setter) {
        if (s.value(key).isBool()) setter(s.value(key).toBool());
    };
    auto number = [&](const char* key, double lo, double hi, auto&& setter) {
        if (s.value(key).isDouble())
        {
            const double v = s.value(key).toDouble();
            if (v >= lo && v <= hi) setter(v);
        }
    };

    if (ModelDisplayMode m; valueOf(modelModes(), s.value("modelDisplayMode"), &m)) view.setModelDisplayMode(m);
    if (RenderDisplayMode r; valueOf(renderModes(), s.value("colors"), &r)) view.setRenderDisplayMode(r);
    boolean("nodes", [&](bool v) { view.setNodesVisible(v); });
    boolean("nodeLabels", [&](bool v) { view.setNodeLabelsVisible(v); });
    boolean("supports", [&](bool v) { view.setSupportsVisible(v); });
    boolean("supportLabels", [&](bool v) { view.setSupportLabelsVisible(v); });
    boolean("grid", [&](bool v) { view.setGridVisible(v); });
    boolean("gridLabels", [&](bool v) { view.setGridLabelsVisible(v); });
    boolean("gridLevels", [&](bool v) { view.setGridLevelsVisible(v); });
    boolean("workPlane", [&](bool v) { view.setWorkPlaneVisible(v); });
    if (s.value("categories").isObject())
    {
        const QJsonObject cats = s.value("categories").toObject();
        for (const auto& [c, n] : categories())
            if (cats.value(QLatin1String(n)).isBool()) view.setElementCategoryVisible(c, cats.value(QLatin1String(n)).toBool());
    }
    boolean("loads", [&](bool v) { view.setLoadsVisible(v); });
    boolean("surfaceLoads", [&](bool v) { view.setSurfaceLoadsVisible(v); });
    boolean("forces", [&](bool v) { view.setForcesVisible(v); });
    boolean("moments", [&](bool v) { view.setMomentsVisible(v); });
    boolean("loadValues", [&](bool v) { view.setLoadValuesVisible(v); });
    number("loadScale", 0.01, 100.0, [&](double v) { view.setLoadScale(v); });
    {
        auto ls = view.loadScaleSettings();
        const auto before = ls;
        TSA::Geometry::LoadScaleMode lm;
        if (valueOf(loadModes(), s.value("loadScaleMode"), &lm)) ls.mode = lm;
        if (s.value("loadMinRatio").isDouble() && s.value("loadMinRatio").toDouble() >= 0.0 && s.value("loadMinRatio").toDouble() <= 1.0)
            ls.minRatio = s.value("loadMinRatio").toDouble();
        if (s.value("loadLegend").isBool()) ls.showLegend = s.value("loadLegend").toBool();
        if (!(ls == before)) view.setLoadScaleSettings(ls);
    }
    if (auto* r = view.resultsVisual())
    {
        boolean("deformed", [&](bool v) { r->setDeformedVisible(v); });
        if (DeformedDisplayMode m; valueOf(deformedModes(), s.value("deformedMode"), &m)) r->setDeformedDisplayMode(m);
        if (ScalePreset p; valueOf(presets(), s.value("deformationPreset"), &p))
            r->setDeformationScalePreset(p, s.value("deformationScale").isDouble() ? s.value("deformationScale").toDouble() : 1.0);
        if (TSA::Geometry::DiagramType d; valueOf(diagrams(), s.value("diagram"), &d)) r->setDiagramType(d);
        number("diagramMultiplier", 1e-3, 1e3, [&](double v) { r->setDiagramScaleMultiplier(v); });
        boolean("diagramLabels", [&](bool v) { r->setDiagramLabelsVisible(v); });
        boolean("reactions", [&](bool v) { r->setReactionsVisible(v); });
        boolean("legend", [&](bool v) { r->setLegendVisible(v); });
        boolean("sectionRotation", [&](bool v) { r->setSectionRotationVisible(v); });
    }
    auto& style = AnnotationStyle::instance();
    if (s.value("annotationMode").toString() == "screen") style.setMode(AnnotationSizeMode::ScreenConstant);
    else if (s.value("annotationMode").toString() == "model") style.setMode(AnnotationSizeMode::ModelLinked);
    number("annotationPixels", 8.0, 32.0, [&](double v) { style.setBasePixels(v); });
    number("annotationModelHeight", 0.01, 10.0, [&](double v) { style.setModelHeight(v); });
    return unknown;
}

QList<DisplayProfile> defaultDisplayProfiles()
{
    const QJsonObject allCats { { "beams", true }, { "columns", true }, { "slabs", true }, { "walls", true },
                                { "foundations", true }, { "trusses", true }, { "cables", true } };
    auto base = [&](const char* mode, const char* colors) {
        return QJsonObject { { "modelDisplayMode", mode }, { "colors", colors }, { "categories", allCats } };
    };
    QList<DisplayProfile> out;
    {
        QJsonObject s = base("physical", "materials");
        s["nodes"] = true; s["nodeLabels"] = false; s["supports"] = true; s["grid"] = true; s["gridLabels"] = true; s["workPlane"] = true;
        s["loads"] = false; s["deformed"] = false; s["diagram"] = "none"; s["reactions"] = false; s["legend"] = false;
        out << DisplayProfile { QString::fromUtf8(QT_TRANSLATE_NOOP("TSA::Viewer::DisplayProfile", "Modélisation")), s };
    }
    {
        QJsonObject s = base("analytical", "structure");
        s["nodes"] = true; s["supports"] = true; s["grid"] = true; s["loads"] = true; s["surfaceLoads"] = true; s["forces"] = true; s["moments"] = true;
        s["loadValues"] = true; s["loadScaleMode"] = "automatic"; s["loadLegend"] = true; s["deformed"] = false; s["diagram"] = "none";
        out << DisplayProfile { QString::fromUtf8(QT_TRANSLATE_NOOP("TSA::Viewer::DisplayProfile", "Charges")), s };
    }
    {
        QJsonObject s = base("finite_element", "analysis");
        s["nodes"] = false; s["nodeLabels"] = false; s["supports"] = true; s["grid"] = false; s["loads"] = false; s["deformed"] = false; s["diagram"] = "none";
        out << DisplayProfile { QString::fromUtf8(QT_TRANSLATE_NOOP("TSA::Viewer::DisplayProfile", "Maillage")), s };
    }
    {
        QJsonObject s = base("analytical", "results");
        s["nodes"] = false; s["supports"] = true; s["grid"] = false; s["loads"] = false; s["deformed"] = false; s["diagram"] = "My";
        s["diagramMultiplier"] = 1.0; s["diagramLabels"] = true; s["reactions"] = true; s["legend"] = true;
        out << DisplayProfile { QString::fromUtf8(QT_TRANSLATE_NOOP("TSA::Viewer::DisplayProfile", "Résultats")), s };
    }
    {
        QJsonObject s = base("analytical", "results");
        s["nodes"] = false; s["grid"] = false; s["loads"] = false; s["deformed"] = true; s["deformedMode"] = "both"; s["deformationPreset"] = "auto";
        s["diagram"] = "none"; s["reactions"] = false; s["legend"] = true; s["sectionRotation"] = true;
        out << DisplayProfile { QString::fromUtf8(QT_TRANSLATE_NOOP("TSA::Viewer::DisplayProfile", "Déformées")), s };
    }
    {
        QJsonObject s = base("analytical", "results");
        s["nodes"] = false; s["grid"] = false; s["loads"] = false; s["supports"] = true; s["deformed"] = true; s["deformedMode"] = "deformed";
        s["deformationPreset"] = "auto"; s["diagram"] = "none"; s["reactions"] = false; s["legend"] = true;
        out << DisplayProfile { QString::fromUtf8(QT_TRANSLATE_NOOP("TSA::Viewer::DisplayProfile", "Analyse modale")), s };
    }
    {
        QJsonObject s = base("physical", "materials");
        s["nodes"] = false; s["nodeLabels"] = false; s["supports"] = false; s["supportLabels"] = false; s["grid"] = false; s["gridLabels"] = false;
        s["gridLevels"] = false; s["workPlane"] = false; s["loads"] = false; s["deformed"] = false; s["diagram"] = "none"; s["reactions"] = false;
        s["legend"] = false;
        out << DisplayProfile { QString::fromUtf8(QT_TRANSLATE_NOOP("TSA::Viewer::DisplayProfile", "Présentation")), s };
    }
    return out;
}

// --- Stockage ---------------------------------------------------------------------------------------------------

DisplayProfileStore::DisplayProfileStore(const QString& group)
    : m_group(group)
{
}

namespace
{
QList<DisplayProfile> readList(const QString& group, bool* present)
{
    QSettings settings;
    const QString raw = settings.value(group + QStringLiteral("/list")).toString();
    *present = settings.contains(group + QStringLiteral("/list"));
    QList<DisplayProfile> out;
    const QJsonArray arr = QJsonDocument::fromJson(raw.toUtf8()).array();   // illisible : liste vide, sans erreur
    for (const QJsonValue& v : arr)
    {
        const QJsonObject o = v.toObject();
        const QString name = o.value("name").toString().trimmed();
        if (name.isEmpty()) continue;
        out << DisplayProfile { name, o.value("settings").toObject() };
    }
    return out;
}
void writeList(const QString& group, const QList<DisplayProfile>& list)
{
    QJsonArray arr;
    for (const auto& p : list) arr.append(QJsonObject { { "name", p.name }, { "settings", p.settings } });
    QSettings settings;
    settings.setValue(group + QStringLiteral("/list"), QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    settings.setValue(group + QStringLiteral("/schema"), 1);
}
} // namespace

void DisplayProfileStore::seedIfEmpty() const
{
    bool present = false;
    readList(m_group, &present);
    if (!present) writeList(m_group, defaultDisplayProfiles());
}

QList<DisplayProfile> DisplayProfileStore::profiles() const
{
    seedIfEmpty();
    bool present = false;
    return readList(m_group, &present);
}

bool DisplayProfileStore::contains(const QString& name) const
{
    for (const auto& p : profiles())
        if (p.name == name) return true;
    return false;
}

DisplayProfile DisplayProfileStore::profile(const QString& name) const
{
    for (const auto& p : profiles())
        if (p.name == name) return p;
    return {};
}

bool DisplayProfileStore::save(const DisplayProfile& profile, QString* error)
{
    const QString name = profile.name.trimmed();
    if (name.isEmpty() || name.size() > 80)
    {
        if (error) *error = QCoreApplication::translate("TSA::Viewer::DisplayProfile", "nom de profil vide ou trop long (80 caractères au plus)");
        return false;
    }
    // Liste blanche : seules les clés graphiques sont enregistrées
    QJsonObject clean;
    const QStringList keys = displayProfileKeys();
    for (auto it = profile.settings.begin(); it != profile.settings.end(); ++it)
        if (keys.contains(it.key())) clean[it.key()] = it.value();
    QList<DisplayProfile> list = profiles();
    bool replaced = false;
    for (auto& p : list)
        if (p.name == name)
        {
            p.settings = clean;
            replaced = true;
        }
    if (!replaced) list << DisplayProfile { name, clean };
    writeList(m_group, list);
    return true;
}

bool DisplayProfileStore::rename(const QString& oldName, const QString& newName, QString* error)
{
    const QString to = newName.trimmed();
    QList<DisplayProfile> list = profiles();
    auto it = std::find_if(list.begin(), list.end(), [&](const DisplayProfile& p) { return p.name == oldName; });
    if (it == list.end())
    {
        if (error) *error = QCoreApplication::translate("TSA::Viewer::DisplayProfile", "profil « %1 » introuvable").arg(oldName);
        return false;
    }
    if (to.isEmpty() || to.size() > 80 || (to != oldName && contains(to)))
    {
        if (error) *error = to.isEmpty() || to.size() > 80 ? QCoreApplication::translate("TSA::Viewer::DisplayProfile", "nom de profil vide ou trop long") : QCoreApplication::translate("TSA::Viewer::DisplayProfile", "un profil « %1 » existe déjà").arg(to);
        return false;
    }
    it->name = to;
    writeList(m_group, list);
    // Documents associés à l'ancien nom : suivent le renommage
    QSettings settings;
    const QString key = m_group + QStringLiteral("/documents");
    QJsonObject map = QJsonDocument::fromJson(settings.value(key).toString().toUtf8()).object();
    for (auto d = map.begin(); d != map.end(); ++d)
        if (d.value().toString() == oldName) d.value() = to;
    settings.setValue(key, QString::fromUtf8(QJsonDocument(map).toJson(QJsonDocument::Compact)));
    return true;
}

bool DisplayProfileStore::remove(const QString& name)
{
    QList<DisplayProfile> list = profiles();
    const auto n = list.size();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const DisplayProfile& p) { return p.name == name; }), list.end());
    if (list.size() == n) return false;
    writeList(m_group, list);
    return true;
}

void DisplayProfileStore::restoreDefaults()
{
    QList<DisplayProfile> list = profiles();
    for (const auto& def : defaultDisplayProfiles())
    {
        auto it = std::find_if(list.begin(), list.end(), [&](const DisplayProfile& p) { return p.name == def.name; });
        if (it != list.end()) it->settings = def.settings;
        else list << def;
    }
    writeList(m_group, list);
}

QString DisplayProfileStore::documentProfile(const QString& filePath) const
{
    if (filePath.isEmpty()) return {};
    QSettings settings;
    const QJsonObject map = QJsonDocument::fromJson(settings.value(m_group + QStringLiteral("/documents")).toString().toUtf8()).object();
    return map.value(normalizedPath(filePath)).toString();
}

void DisplayProfileStore::setDocumentProfile(const QString& filePath, const QString& name)
{
    if (filePath.isEmpty()) return;
    QSettings settings;
    const QString key = m_group + QStringLiteral("/documents");
    QJsonObject map = QJsonDocument::fromJson(settings.value(key).toString().toUtf8()).object();
    if (name.isEmpty()) map.remove(normalizedPath(filePath));
    else map[normalizedPath(filePath)] = name;
    settings.setValue(key, QString::fromUtf8(QJsonDocument(map).toJson(QJsonDocument::Compact)));
}

} // namespace TSA::Viewer
