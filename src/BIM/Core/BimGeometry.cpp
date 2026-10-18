#include "BimGeometry.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QIODevice>

#include <algorithm>
#include <cmath>
#include <limits>

namespace TSA::BIM
{

namespace
{
constexpr quint32 kMagic = 0x4F454742;   // 'BGEO'
constexpr quint16 kVersion = 1;
}

bool TriangleMesh::valid() const
{
    if (positions.size() % 3 != 0 || indices.size() % 3 != 0 || indices.empty()) return false;
    const auto n = static_cast<std::uint32_t>(vertexCount());
    if (std::any_of(indices.begin(), indices.end(), [n](std::uint32_t i) { return i >= n; })) return false;
    return std::all_of(positions.begin(), positions.end(), [](float v) { return std::isfinite(v); })
        && std::all_of(origin.begin(), origin.end(), [](double v) { return std::isfinite(v); });
}

std::array<double, 3> TriangleMesh::vertex(std::size_t i) const
{
    return { origin[0] + positions[3 * i], origin[1] + positions[3 * i + 1], origin[2] + positions[3 * i + 2] };
}

std::array<double, 6> TriangleMesh::bounds() const
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    std::array<double, 6> b { inf, inf, inf, -inf, -inf, -inf };
    for (std::size_t i = 0; i < vertexCount(); ++i)
    {
        const auto p = vertex(i);
        for (int k = 0; k < 3; ++k)
        {
            b[k] = std::min(b[k], p[k]);
            b[3 + k] = std::max(b[3 + k], p[k]);
        }
    }
    return b;
}

QByteArray serializeGeometries(const std::map<int, MeshPtr>& meshes)
{
    QByteArray out;
    QDataStream s(&out, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s.setFloatingPointPrecision(QDataStream::DoublePrecision);
    // Nombre annoncé = maillages réellement écrits (un pointeur nul sauté décalerait toute la relecture)
    const auto written = std::count_if(meshes.begin(), meshes.end(), [](const auto& entry) { return entry.second != nullptr; });
    s << kMagic << kVersion << static_cast<quint32>(written);
    for (const auto& [id, m] : meshes)
    {
        if (!m) continue;
        s << static_cast<qint32>(id) << m->origin[0] << m->origin[1] << m->origin[2];
        for (float c : m->color) s << static_cast<double>(c);
        s << QByteArray::fromStdString(m->representation);
        s << static_cast<quint32>(m->positions.size()) << static_cast<quint32>(m->indices.size());
        // Blocs bruts (little-endian sur toutes les plateformes visées : x64, arm64)
        s.writeRawData(reinterpret_cast<const char*>(m->positions.data()), static_cast<int>(m->positions.size() * sizeof(float)));
        s.writeRawData(reinterpret_cast<const char*>(m->indices.data()), static_cast<int>(m->indices.size() * sizeof(std::uint32_t)));
    }
    return out;
}

bool deserializeGeometries(const QByteArray& data, std::map<int, MeshPtr>& out, std::string* error)
{
    auto fail = [error](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };
    QDataStream s(data);
    s.setByteOrder(QDataStream::LittleEndian);
    s.setFloatingPointPrecision(QDataStream::DoublePrecision);
    quint32 magic = 0, count = 0;
    quint16 version = 0;
    s >> magic >> version >> count;
    if (magic != kMagic) return fail(QCoreApplication::translate("TSA::BIM::BimGeometry", "géométries : signature BGEO absente").toStdString());
    if (version > kVersion) return fail(QCoreApplication::translate("TSA::BIM::BimGeometry", "géométries : version future").toStdString());
    for (quint32 k = 0; k < count && s.status() == QDataStream::Ok; ++k)
    {
        qint32 id = 0;
        auto m = std::make_shared<TriangleMesh>();
        s >> id >> m->origin[0] >> m->origin[1] >> m->origin[2];
        for (float& c : m->color)
        {
            double v = 0;
            s >> v;
            c = static_cast<float>(v);
        }
        QByteArray rep;
        quint32 np = 0, ni = 0;
        s >> rep >> np >> ni;
        // Garde-fou contre un fichier corrompu : tailles cohérentes avec le reste du flux
        const qint64 need = static_cast<qint64>(np) * sizeof(float) + static_cast<qint64>(ni) * sizeof(std::uint32_t);
        if (s.status() != QDataStream::Ok || need > s.device()->bytesAvailable()) return fail(QCoreApplication::translate("TSA::BIM::BimGeometry", "géométries : bloc tronqué").toStdString());
        m->representation = rep.toStdString();
        m->positions.resize(np);
        m->indices.resize(ni);
        s.readRawData(reinterpret_cast<char*>(m->positions.data()), static_cast<int>(np * sizeof(float)));
        s.readRawData(reinterpret_cast<char*>(m->indices.data()), static_cast<int>(ni * sizeof(std::uint32_t)));
        if (m->valid()) out[id] = std::move(m);   // entrée invalide ignorée, pas le reste
    }
    return s.status() == QDataStream::Ok || fail(QCoreApplication::translate("TSA::BIM::BimGeometry", "géométries : flux illisible").toStdString());
}

} // namespace TSA::BIM
