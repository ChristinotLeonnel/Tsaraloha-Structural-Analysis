#include "WorkPlane.h"
#include <gp_Lin.hxx>
#include <gp_Trsf.hxx>
#include <IntAna_IntConicQuad.hxx>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <cmath>
#include <algorithm>

namespace TSA::Coordinate
{

constexpr double PI_VAL = 3.14159265358979323846;
constexpr double RAD_TO_DEG = 180.0 / PI_VAL;
constexpr double DEG_TO_RAD = PI_VAL / 180.0;

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

void WorkPlane::setOrigin(const gp_Pnt& orig)
{
    m_cs.SetLocation(orig);
    m_plane = gp_Pln(m_cs);
    if (m_type == WorkPlaneType::GlobalXY || m_type == WorkPlaneType::ElevationZ)
    {
        m_offset = orig.Z();
    }
    else if (m_type == WorkPlaneType::GlobalXZ)
    {
        m_offset = orig.Y();
    }
    else if (m_type == WorkPlaneType::GlobalYZ)
    {
        m_offset = orig.X();
    }
}

double WorkPlane::rotationX() const noexcept
{
    gp_Dir d = normal();
    return std::atan2(d.Y(), d.Z()) * RAD_TO_DEG;
}

double WorkPlane::rotationY() const noexcept
{
    gp_Dir d = normal();
    return std::asin(std::clamp(-d.X(), -1.0, 1.0)) * RAD_TO_DEG;
}

double WorkPlane::rotationZ() const noexcept
{
    gp_Dir xd = xDirection();
    return std::atan2(xd.Y(), xd.X()) * RAD_TO_DEG;
}

void WorkPlane::setRotation(double rxDeg, double ryDeg, double rzDeg)
{
    double rx = rxDeg * DEG_TO_RAD;
    double ry = ryDeg * DEG_TO_RAD;
    double rz = rzDeg * DEG_TO_RAD;

    // Matrice de rotation Euler Rz * Ry * Rx
    gp_Trsf rotX, rotY, rotZ;
    rotX.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(1,0,0)), rx);
    rotY.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,1,0)), ry);
    rotZ.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(0,0,1)), rz);

    gp_Trsf totalRot = rotZ * rotY * rotX;

    gp_Dir baseNormal(0, 0, 1);
    gp_Dir baseX(1, 0, 0);

    baseNormal.Transform(totalRot);
    baseX.Transform(totalRot);

    gp_Pnt curOrigin = origin();
    m_cs = gp_Ax3(curOrigin, baseNormal, baseX);
    m_plane = gp_Pln(m_cs);
    m_type = WorkPlaneType::Custom;
}

void WorkPlane::setDimensions(double w, double h) noexcept
{
    m_width = std::max(0.5, w);
    m_height = std::max(0.5, h);
}

void WorkPlane::setGridSettings(double spX, double spY, int subdivisions, bool visible) noexcept
{
    m_gridSpacingX = std::max(0.01, spX);
    m_gridSpacingY = std::max(0.01, spY);
    m_gridSubdivisions = std::max(1, subdivisions);
    m_isGridVisible = visible;
}

void WorkPlane::translate(const gp_Vec& vec)
{
    m_cs.Translate(vec);
    m_plane = gp_Pln(m_cs);
    if (m_type == WorkPlaneType::GlobalXY || m_type == WorkPlaneType::ElevationZ)
    {
        m_offset = m_cs.Location().Z();
    }
    else if (m_type == WorkPlaneType::GlobalXZ)
    {
        m_offset = m_cs.Location().Y();
    }
    else if (m_type == WorkPlaneType::GlobalYZ)
    {
        m_offset = m_cs.Location().X();
    }
}

void WorkPlane::rotate(const gp_Pnt& center, const gp_Dir& axis, double angleRad)
{
    gp_Trsf rotTrsf;
    rotTrsf.SetRotation(gp_Ax1(center, axis), angleRad);
    transform(rotTrsf);
}

void WorkPlane::transform(const gp_Trsf& trsf)
{
    m_cs.Transform(trsf);
    m_plane = gp_Pln(m_cs);
    m_type = WorkPlaneType::Custom;
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
    j["id"] = m_id;
    j["type"] = static_cast<int>(m_type);
    j["name"] = QString::fromStdString(m_name);
    j["offset"] = m_offset;
    j["width"] = m_width;
    j["height"] = m_height;
    j["gridSpacingX"] = m_gridSpacingX;
    j["gridSpacingY"] = m_gridSpacingY;
    j["gridSubdivisions"] = m_gridSubdivisions;
    j["isGridVisible"] = m_isGridVisible;
    j["isVisible"] = m_isVisible;
    j["isActive"] = m_isActive;
    j["isLocked"] = m_isLocked;
    j["isIsolated"] = m_isIsolated;
    j["isolationDistance"] = m_isolationDistance;

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

    WorkPlane wp(t, n, off);
    wp.setId(j.value("id").toInt(1));
    wp.setDimensions(j.value("width").toDouble(20.0), j.value("height").toDouble(20.0));
    wp.setGridSettings(j.value("gridSpacingX").toDouble(1.0),
                       j.value("gridSpacingY").toDouble(1.0),
                       j.value("gridSubdivisions").toInt(5),
                       j.value("isGridVisible").toBool(true));
    wp.setVisible(j.value("isVisible").toBool(true));
    wp.setActive(j.value("isActive").toBool(true));
    wp.setLocked(j.value("isLocked").toBool(false));
    wp.setIsolated(j.value("isIsolated").toBool(false));
    wp.setIsolationDistance(j.value("isolationDistance").toDouble(1.5));

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
            wp.setCoordinateSystem(cs);
            wp.setType(t);
            wp.setName(n);
            wp.m_offset = off;
        }
    }

    return wp;
}

} // namespace TSA::Coordinate
