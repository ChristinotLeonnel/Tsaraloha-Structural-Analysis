#include "NamedViews.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

namespace TSA::Model
{

int NamedViewSet::indexOf(const std::string& name) const
{
    for (size_t i = 0; i < views.size(); ++i)
        if (views[i].name == name)
            return static_cast<int>(i);
    return -1;
}

int NamedViewSet::upsert(const NamedView& view)
{
    const int i = indexOf(view.name);
    if (i >= 0)
    {
        views[static_cast<size_t>(i)] = view;
        return i;
    }
    views.push_back(view);
    return static_cast<int>(views.size()) - 1;
}

bool NamedViewSet::rename(const std::string& from, const std::string& to)
{
    const int i = indexOf(from);
    if (i < 0 || to.empty() || (to != from && indexOf(to) >= 0))
        return false;
    views[static_cast<size_t>(i)].name = to;
    return true;
}

bool NamedViewSet::remove(const std::string& name)
{
    const int i = indexOf(name);
    if (i < 0)
        return false;
    views.erase(views.begin() + i);
    return true;
}

namespace
{
QJsonArray vec(const std::array<double, 3>& v) { return QJsonArray { v[0], v[1], v[2] }; }

bool readVec(const QJsonValue& value, std::array<double, 3>& out)
{
    const QJsonArray a = value.toArray();
    if (a.size() != 3)
        return false;
    for (int k = 0; k < 3; ++k)
    {
        if (!a[k].isDouble() || !std::isfinite(a[k].toDouble()))
            return false;
        out[static_cast<size_t>(k)] = a[k].toDouble();
    }
    return true;
}
} // namespace

std::string NamedViewSet::toJson() const
{
    QJsonArray list;
    for (const NamedView& v : views)
    {
        QJsonObject o;
        o["name"] = QString::fromStdString(v.name);
        o["eye"] = vec(v.eye);
        o["center"] = vec(v.center);
        o["up"] = vec(v.up);
        o["scale"] = v.scale;
        o["projection"] = v.orthographic ? "orthographic" : "perspective";
        o["mode2D"] = v.mode2D;
        o["planFace"] = QString::fromStdString(v.planFace);
        if (!v.workPlaneJson.empty())
            o["workPlane"] = QString::fromStdString(v.workPlaneJson);
        list.append(o);
    }
    QJsonObject root;
    root["schema"] = 1;
    root["views"] = list;
    return QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
}

NamedViewSet NamedViewSet::fromJson(const std::string& json, std::string* warnings)
{
    NamedViewSet set;
    if (json.empty())
        return set;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isObject())
    {
        if (warnings) *warnings += QCoreApplication::translate("TSA::Model::NamedViews", "vues nommées : JSON illisible, ignoré").toStdString() + "\n";
        return set;
    }
    for (const QJsonValue& value : doc.object()["views"].toArray())
    {
        const QJsonObject o = value.toObject();
        NamedView v;
        v.name = o["name"].toString().toStdString();
        v.scale = o["scale"].toDouble(-1.0);
        const bool ok = !v.name.empty() && set.indexOf(v.name) < 0 && readVec(o["eye"], v.eye) && readVec(o["center"], v.center)
                        && readVec(o["up"], v.up) && v.scale > 0.0 && std::isfinite(v.scale);
        if (!ok)
        {
            if (warnings)
                *warnings += QCoreApplication::translate("TSA::Model::NamedViews", "vue nommée invalide ou en double ignorée : « %1 »")
                                 .arg(QString::fromStdString(v.name))
                                 .toStdString()
                             + "\n";
            continue;
        }
        v.orthographic = o["projection"].toString("orthographic") != "perspective";
        v.mode2D = o["mode2D"].toBool(false);
        v.planFace = o["planFace"].toString("custom").toStdString();
        v.workPlaneJson = o["workPlane"].toString().toStdString();
        set.views.push_back(v);
    }
    return set;
}

} // namespace TSA::Model
