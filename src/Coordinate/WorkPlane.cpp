#include "WorkPlane.h"
#include <gp_Lin.hxx>
#include <IntAna_IntConicQuad.hxx>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <cmath>

namespace TSA::Coordinate
{

WorkPlane::WorkPlane()
    : m_type(WorkPlaneType::GlobalXY)
    , m_name("Plan XY")
    , m_offset(0.0)
    , m_cs(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0))
    , m_plane(m_cs)
{
}

WorkPlane::WorkPlane(WorkPlaneType type, const std::string& name, double offset)
    : m_type(type)
    , m_name(name)
    , m_offset(offset)
    , m_cs(gp_Pnt(0.0, 0.0, offset), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0))
    , m_plane(m_cs)
{
    updatePlane();
}

WorkPlane::WorkPlane(const gp_Ax3& coordinateSystem, const std::string& name, WorkPlaneType type)
    : m_type(type)
    , m_name(name)
    , m_offset(0.0)
    , m_cs(coordinateSystem)
    , m_plane(m_cs)
{
}

void WorkPlane::setOffset(double off)
{
    m_offset = off;
    updatePlane();
}

void WorkPlane::setCoordinateSystem(const gp_Ax3& cs)
{
    m_cs = cs;
    m_plane = gp_Pln(m_cs);
}

void WorkPlane::updatePlane()
{
    switch (m_type)
    {
    case WorkPlaneType::GlobalXZ:
        m_cs = gp_Ax3(gp_Pnt(0.0, m_offset, 0.0), gp_Dir(0.0, 1.0, 0.0), gp_Dir(1.0, 0.0, 0.0));
        break;
    case WorkPlaneType::GlobalYZ:
        m_cs = gp_Ax3(gp_Pnt(m_offset, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0));
        break;
    case WorkPlaneType::GlobalXY:
    case WorkPlaneType::ElevationZ:
    default:
        m_cs = gp_Ax3(gp_Pnt(0.0, 0.0, m_offset), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0));
        break;
    }
    m_plane = gp_Pln(m_cs);
}

bool WorkPlane::projectRay(const gp_Pnt& eye, const gp_Dir& rayDir, gp_Pnt& outPnt) const
{
    gp_Lin line(eye, rayDir);
    IntAna_IntConicQuad inter(line, m_plane, 1e-6);
    if (inter.IsDone() && !inter.IsParallel() && inter.NbPoints() > 0)
    {
        outPnt = inter.Point(1);
        return true;
    }

    // Fallback analytique direct : (P - P0) . N = 0 => (Eye + t*Dir - P0) . N = 0
    gp_Dir N = normal();
    double denom = rayDir.X() * N.X() + rayDir.Y() * N.Y() + rayDir.Z() * N.Z();
    if (std::abs(denom) > 1e-7)
    {
        gp_Pnt P0 = origin();
        double numer = (P0.X() - eye.X()) * N.X() + (P0.Y() - eye.Y()) * N.Y() + (P0.Z() - eye.Z()) * N.Z();
        double t = numer / denom;
        outPnt = gp_Pnt(eye.X() + t * rayDir.X(),
                        eye.Y() + t * rayDir.Y(),
                        eye.Z() + t * rayDir.Z());
        return true;
    }

    return false;
}

gp_Pnt WorkPlane::projectOrtho(const gp_Pnt& worldPoint) const
{
    gp_Dir N = normal();
    gp_Pnt P0 = origin();
    double dist = (worldPoint.X() - P0.X()) * N.X() +
                  (worldPoint.Y() - P0.Y()) * N.Y() +
                  (worldPoint.Z() - P0.Z()) * N.Z();

    return gp_Pnt(worldPoint.X() - dist * N.X(),
                  worldPoint.Y() - dist * N.Y(),
                  worldPoint.Z() - dist * N.Z());
}

double WorkPlane::distanceTo(const gp_Pnt& worldPoint) const
{
    return m_plane.Distance(worldPoint);
}

gp_Pnt WorkPlane::toUcs(const gp_Pnt& worldPoint) const
{
    gp_Pnt P0 = origin();
    gp_Vec V(P0, worldPoint);
    gp_Dir Xd = xDirection();
    gp_Dir Yd = yDirection();
    gp_Dir Zd = normal();

    double u = V.Dot(gp_Vec(Xd));
    double v = V.Dot(gp_Vec(Yd));
    double w = V.Dot(gp_Vec(Zd));

    return gp_Pnt(u, v, w);
}

gp_Pnt WorkPlane::toWorld(const gp_Pnt& ucsPoint) const
{
    gp_Pnt P0 = origin();
    gp_Dir Xd = xDirection();
    gp_Dir Yd = yDirection();
    gp_Dir Zd = normal();

    gp_Vec Vu = gp_Vec(Xd) * ucsPoint.X();
    gp_Vec Vv = gp_Vec(Yd) * ucsPoint.Y();
    gp_Vec Vw = gp_Vec(Zd) * ucsPoint.Z();

    return P0.Translated(Vu).Translated(Vv).Translated(Vw);
}

WorkPlane WorkPlane::xy(double elevation, const std::string& name)
{
    return WorkPlane(WorkPlaneType::GlobalXY, name.empty() ? "Plan XY" : name, elevation);
}

WorkPlane WorkPlane::xz(double yOffset, const std::string& name)
{
    return WorkPlane(WorkPlaneType::GlobalXZ, name.empty() ? "Plan XZ" : name, yOffset);
}

WorkPlane WorkPlane::yz(double xOffset, const std::string& name)
{
    return WorkPlane(WorkPlaneType::GlobalYZ, name.empty() ? "Plan YZ" : name, xOffset);
}

WorkPlane WorkPlane::fromThreePoints(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3, const std::string& name)
{
    gp_Vec v12(p1, p2);
    gp_Vec v13(p1, p3);
    gp_Vec normalVec = v12.Crossed(v13);

    if (normalVec.SquareMagnitude() < 1e-10)
    {
        // Points colinéaires : fallback sur plan horizontal
        return WorkPlane::xy(p1.Z(), name);
    }

    gp_Dir normDir(normalVec);
    gp_Dir xDir(v12);
    gp_Ax3 cs(p1, normDir, xDir);

    WorkPlane wp(cs, name.empty() ? "Plan 3 Points" : name, WorkPlaneType::ThreePoints);
    return wp;
}

WorkPlane WorkPlane::fromOriginAndNormal(const gp_Pnt& origin, const gp_Dir& normal, const std::string& name)
{
    gp_Ax3 cs(origin, normal);
    WorkPlane wp(cs, name.empty() ? "Plan Personnalisé" : name, WorkPlaneType::Custom);
    return wp;
}

std::string WorkPlane::serializeToJson() const
{
    QJsonObject j;
    j["type"] = static_cast<int>(m_type);
    j["name"] = QString::fromStdString(m_name);
    j["offset"] = m_offset;

    QJsonArray orig;
    orig.append(origin().X());
    orig.append(origin().Y());
    orig.append(origin().Z());
    j["origin"] = orig;

    QJsonArray norm;
    norm.append(normal().X());
    norm.append(normal().Y());
    norm.append(normal().Z());
    j["normal"] = norm;

    QJsonArray xd;
    xd.append(xDirection().X());
    xd.append(xDirection().Y());
    xd.append(xDirection().Z());
    j["xDir"] = xd;

    QJsonDocument doc(j);
    return doc.toJson(QJsonDocument::Indented).toStdString();
}

WorkPlane WorkPlane::deserializeFromJson(const std::string& jsonStr)
{
    QByteArray bytes = QByteArray::fromRawData(jsonStr.data(), static_cast<int>(jsonStr.size()));
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        return WorkPlane();
    }

    QJsonObject j = doc.object();
    WorkPlaneType t = static_cast<WorkPlaneType>(j.value("type").toInt(0));
    std::string n = j.value("name").toString("Plan XY").toStdString();
    double off = j.value("offset").toDouble(0.0);

    if (j.contains("origin") && j.contains("normal") && j.contains("xDir"))
    {
        QJsonArray o = j.value("origin").toArray();
        QJsonArray nm = j.value("normal").toArray();
        QJsonArray xd = j.value("xDir").toArray();
        if (o.size() >= 3 && nm.size() >= 3 && xd.size() >= 3)
        {
            gp_Pnt p0(o[0].toDouble(), o[1].toDouble(), o[2].toDouble());
            gp_Dir norm(nm[0].toDouble(), nm[1].toDouble(), nm[2].toDouble());
            gp_Dir xDir(xd[0].toDouble(), xd[1].toDouble(), xd[2].toDouble());
            gp_Ax3 cs(p0, norm, xDir);
            WorkPlane wp(cs, n, t);
            wp.m_offset = off;
            return wp;
        }
    }

    return WorkPlane(t, n, off);
}

} // namespace TSA::Coordinate
