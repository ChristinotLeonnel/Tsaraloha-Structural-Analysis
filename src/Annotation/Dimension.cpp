#include "Dimension.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace TSA::Annotation
{

namespace
{
void warn(std::string* w, const std::string& text)
{
    if (!w) return;
    if (!w->empty()) *w += "\n";
    *w += text;
}

QJsonArray xyz(const std::array<double, 3>& p)
{
    return QJsonArray{ p[0], p[1], p[2] };
}

bool readXyz(const QJsonValue& v, std::array<double, 3>& out)
{
    const QJsonArray a = v.toArray();
    if (a.size() != 3) return false;
    for (int i = 0; i < 3; ++i)
    {
        if (!a[i].isDouble()) return false;
        out[i] = a[i].toDouble();
    }
    return true;
}

template <typename E>
bool fromKey(const QString& key, E& out, std::initializer_list<E> values, std::string (*name)(E))
{
    for (E v : values)
        if (key.toStdString() == name(v)) { out = v; return true; }
    return false;
}
std::string kindName(DimensionKind k) { return kindKey(k); }
std::string axisName(MeasureAxis a) { return axisKey(a); }
} // namespace

bool Dimension::hasInvalidReference() const
{
    return std::any_of(anchors.begin(), anchors.end(), [](const DimensionAnchor& a) { return a.orphaned; });
}

int Dimension::requiredAnchors(DimensionKind kind)
{
    switch (kind)
    {
    case DimensionKind::Linear: return 2;
    case DimensionKind::Angular: return 3;
    case DimensionKind::Level: return 1;
    default: return 2;
    }
}

std::string DimensionStyle::unitSymbol() const
{
    switch (unit)
    {
    case LengthUnit::Centimeter: return "cm";
    case LengthUnit::Millimeter: return "mm";
    default: return "m";
    }
}

double DimensionStyle::unitFactor() const
{
    switch (unit)
    {
    case LengthUnit::Centimeter: return 100.0;
    case LengthUnit::Millimeter: return 1000.0;
    default: return 1.0;
    }
}

std::string kindKey(DimensionKind k)
{
    switch (k)
    {
    case DimensionKind::Linear: return "linear";
    case DimensionKind::Angular: return "angular";
    case DimensionKind::Level: return "level";
    case DimensionKind::Chain: return "chain";
    case DimensionKind::Cumulative: return "cumulative";
    }
    return "linear";
}

std::string axisKey(MeasureAxis a)
{
    switch (a)
    {
    case MeasureAxis::Aligned: return "aligned";
    case MeasureAxis::X: return "x";
    case MeasureAxis::Y: return "y";
    case MeasureAxis::Z: return "z";
    case MeasureAxis::Horizontal: return "horizontal";
    }
    return "aligned";
}

std::string kindDisplayName(DimensionKind k, MeasureAxis a)
{
    switch (k)
    {
    case DimensionKind::Angular: return "Cotation angulaire";
    case DimensionKind::Level: return "Cotation de niveau";
    case DimensionKind::Chain: return "Cotation en chaîne";
    case DimensionKind::Cumulative: return "Cotation cumulée";
    case DimensionKind::Linear:
        switch (a)
        {
        case MeasureAxis::X: return "Cotation suivant X";
        case MeasureAxis::Y: return "Cotation suivant Y";
        case MeasureAxis::Z: return "Cotation verticale (Z)";
        case MeasureAxis::Horizontal: return "Cotation horizontale";
        default: return "Cotation alignée";
        }
    }
    return "Cotation";
}

std::string DimensionSet::toJson() const
{
    QJsonObject root;
    root["schema"] = 1;
    root["nextId"] = nextId;
    QJsonObject s;
    s["unit"] = QString::fromStdString(style.unitSymbol());
    s["decimals"] = style.decimals;
    s["rounding"] = style.rounding;
    s["showUnit"] = style.showUnit;
    s["angleDecimals"] = style.angleDecimals;
    s["textHeightPx"] = style.textHeightPx;
    s["arrowSizePx"] = style.arrowSizePx;
    s["extensionGap"] = style.extensionGap;
    s["extensionOvershoot"] = style.extensionOvershoot;
    s["levelReference"] = style.levelReference;
    s["color"] = QString::fromStdString(style.color);
    s["invalidColor"] = QString::fromStdString(style.invalidColor);
    s["textInPlane"] = style.textInPlane;
    s["visible"] = style.visible;
    root["style"] = s;

    QJsonArray list;
    for (const auto& [id, d] : items)
    {
        QJsonObject o;
        o["id"] = d.id;
        o["kind"] = QString::fromStdString(kindKey(d.kind));
        o["axis"] = QString::fromStdString(axisKey(d.axis));
        QJsonArray anchors;
        for (const auto& a : d.anchors)
        {
            QJsonObject ao;
            ao["node"] = a.nodeId;
            ao["point"] = xyz(a.point);
            if (a.orphaned) ao["orphaned"] = true;
            anchors.append(ao);
        }
        o["anchors"] = anchors;
        o["position"] = xyz(d.position);
        if (!d.textOverride.empty()) o["text"] = QString::fromStdString(d.textOverride);
        if (!d.color.empty()) o["color"] = QString::fromStdString(d.color);
        list.append(o);
    }
    root["dimensions"] = list;
    return QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
}

DimensionSet DimensionSet::fromJson(const std::string& json, std::string* warnings)
{
    DimensionSet set;
    if (json.empty()) return set; // ancien projet : aucune cotation
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        warn(warnings, "Cotations illisibles : ignorées.");
        return set;
    }
    const QJsonObject root = doc.object();
    if (root["schema"].toInt(0) > 1) warn(warnings, "Cotations d'une version plus récente de TSA : champs inconnus ignorés.");

    const QJsonObject s = root["style"].toObject();
    const QString unit = s["unit"].toString("m");
    set.style.unit = unit == "mm" ? LengthUnit::Millimeter : (unit == "cm" ? LengthUnit::Centimeter : LengthUnit::Meter);
    set.style.decimals = std::clamp(s["decimals"].toInt(3), 0, 6);
    set.style.rounding = std::max(0.0, s["rounding"].toDouble(0.0));
    set.style.showUnit = s["showUnit"].toBool(true);
    set.style.angleDecimals = std::clamp(s["angleDecimals"].toInt(1), 0, 4);
    set.style.textHeightPx = std::clamp(s["textHeightPx"].toDouble(14.0), 6.0, 72.0);
    set.style.arrowSizePx = std::clamp(s["arrowSizePx"].toDouble(12.0), 2.0, 60.0);
    set.style.extensionGap = std::max(0.0, s["extensionGap"].toDouble(0.05));
    set.style.extensionOvershoot = std::max(0.0, s["extensionOvershoot"].toDouble(0.10));
    set.style.levelReference = s["levelReference"].toDouble(0.0);
    if (s.contains("color")) set.style.color = s["color"].toString().toStdString();
    if (s.contains("invalidColor")) set.style.invalidColor = s["invalidColor"].toString().toStdString();
    set.style.textInPlane = s["textInPlane"].toBool(false);
    set.style.visible = s["visible"].toBool(true);

    int maxId = 0, skipped = 0;
    for (const auto& v : root["dimensions"].toArray())
    {
        const QJsonObject o = v.toObject();
        Dimension d;
        d.id = o["id"].toInt(0);
        bool ok = d.id > 0 && !set.items.count(d.id);
        ok = ok && fromKey(o["kind"].toString(), d.kind,
                           { DimensionKind::Linear, DimensionKind::Angular, DimensionKind::Level, DimensionKind::Chain,
                             DimensionKind::Cumulative },
                           &kindName);
        if (!fromKey(o["axis"].toString("aligned"), d.axis,
                     { MeasureAxis::Aligned, MeasureAxis::X, MeasureAxis::Y, MeasureAxis::Z, MeasureAxis::Horizontal }, &axisName))
            d.axis = MeasureAxis::Aligned;
        for (const auto& av : o["anchors"].toArray())
        {
            const QJsonObject ao = av.toObject();
            DimensionAnchor a;
            a.nodeId = ao["node"].toInt(-1);
            a.orphaned = ao["orphaned"].toBool(false);
            ok = ok && readXyz(ao["point"], a.point);
            d.anchors.push_back(a);
        }
        ok = ok && readXyz(o["position"], d.position) &&
             static_cast<int>(d.anchors.size()) >= Dimension::requiredAnchors(d.kind) &&
             ((d.kind != DimensionKind::Linear && d.kind != DimensionKind::Angular && d.kind != DimensionKind::Level) ||
              static_cast<int>(d.anchors.size()) == Dimension::requiredAnchors(d.kind));
        if (!ok)
        {
            ++skipped;
            continue;
        }
        d.textOverride = o["text"].toString().toStdString();
        d.color = o["color"].toString().toStdString();
        maxId = std::max(maxId, d.id);
        set.items[d.id] = std::move(d);
    }
    set.nextId = std::max(root["nextId"].toInt(1), maxId + 1);
    if (skipped) warn(warnings, std::to_string(skipped) + " cotation(s) invalide(s) ignorée(s).");
    return set;
}

} // namespace TSA::Annotation
