#include "MultiEditSession.h"
#include "Model.h"

namespace TSA::Model
{

namespace
{
bool sameSection(const Section& a, const Section& b)
{
    return a.id == b.id && a.name == b.name && a.shape == b.shape && a.width == b.width && a.height == b.height
        && a.diameter == b.diameter && a.tw == b.tw && a.tf == b.tf;
}

bool sameMaterial(const Material& a, const Material& b)
{
    return a.id == b.id && a.name == b.name && a.E == b.E && a.nu == b.nu && a.density == b.density && a.fk == b.fk;
}

bool sameRelease(const EndRelease& a, const EndRelease& b)
{
    return a.fx == b.fx && a.fy == b.fy && a.fz == b.fz && a.mx == b.mx && a.my == b.my && a.mz == b.mz;
}

template <typename T>
bool same(const T& a, const T& b) { return a == b; }
bool same(const Section& a, const Section& b) { return sameSection(a, b); }
bool same(const Material& a, const Material& b) { return sameMaterial(a, b); }
bool same(const EndRelease& a, const EndRelease& b) { return sameRelease(a, b); }

} // namespace

bool MultiEditSession::supports(ElementKind kind) noexcept
{
    return kind != ElementKind::Cable;
}

void MultiEditSession::begin(const Model& model, ElementKind kind, int primaryId, std::set<int> others)
{
    clear();
    others.erase(primaryId);
    if (!supports(kind) || others.empty()) return;
    m_kind = kind;
    m_primaryId = primaryId;
    m_others = std::move(others);
    capture(model);
}

void MultiEditSession::clear() noexcept
{
    m_kind.reset();
    m_primaryId = 0;
    m_others.clear();
    m_before = std::monostate{};
}

void MultiEditSession::capture(const Model& model)
{
    m_before = std::monostate{};
    if (!m_kind) return;
    switch (*m_kind)
    {
    case ElementKind::Node: if (const auto* e = model.getNode(m_primaryId)) m_before = *e; break;
    case ElementKind::Beam: if (const auto* e = model.getBeam(m_primaryId)) m_before = *e; break;
    case ElementKind::Column: if (const auto* e = model.getColumn(m_primaryId)) m_before = *e; break;
    case ElementKind::TrussMember: if (const auto* e = model.getTrussMember(m_primaryId)) m_before = *e; break;
    case ElementKind::Slab: if (const auto* e = model.getSlab(m_primaryId)) m_before = *e; break;
    case ElementKind::Wall: if (const auto* e = model.getWall(m_primaryId)) m_before = *e; break;
    case ElementKind::Foundation: if (const auto* e = model.getFoundation(m_primaryId)) m_before = *e; break;
    default: break;
    }
}

// Reporte un champ s'il a changé sur l'élément principal (b = avant, a = après, d = destination).
#define TSA_FIELD(get, set)                     if (!same(b.get(), a.get()))                {                                               d.set(a.get());                             c = true;                               }

ModelDiff MultiEditSession::propagate(Model& model)
{
    ModelDiff diff;
    if (!active() || std::holds_alternative<std::monostate>(m_before)) return diff;

    // Compare l'élément principal à son état précédent et reporte les champs modifiés.
    auto run = [&](auto getter, auto& before, ElementDiff& kindDiff, std::vector<int>& modifiedIds, auto copyFields) {
        const auto* afterPtr = getter(m_primaryId);
        if (!afterPtr) return;
        const auto after = *afterPtr;
        for (int id : m_others)
        {
            auto* dst = getter(id);
            bool changed = false;
            if (dst) copyFields(before, after, *dst, changed);
            if (changed)
            {
                kindDiff.modified.push_back(id);
                modifiedIds.push_back(id);
            }
        }
        before = after;
    };

    switch (*m_kind)
    {
    case ElementKind::Node:
        run([&](int id) { return model.getNode(id); }, std::get<Node>(m_before), diff.nodes, diff.modifiedNodeIds,
            [](const Node& b, const Node& a, Node& d, bool& c) {
                TSA_FIELD(support, setSupport)
                TSA_FIELD(color, setColor)
            });
        break;
    case ElementKind::Beam:
        run([&](int id) { return model.getBeam(id); }, std::get<Beam>(m_before), diff.beams, diff.modifiedBeamIds,
            [](const Beam& b, const Beam& a, Beam& d, bool& c) {
                TSA_FIELD(role, setRole)
                TSA_FIELD(section, setSection)
                TSA_FIELD(material, setMaterial)
                TSA_FIELD(rotation, setRotation)
                TSA_FIELD(eccentricity, setEccentricity)
                TSA_FIELD(startRelease, setStartRelease)
                TSA_FIELD(endRelease, setEndRelease)
                TSA_FIELD(color, setColor)
            });
        break;
    case ElementKind::Column:
        run([&](int id) { return model.getColumn(id); }, std::get<Column>(m_before), diff.columns, diff.modifiedColumnIds,
            [](const Column& b, const Column& a, Column& d, bool& c) {
                TSA_FIELD(section, setSection)
                TSA_FIELD(material, setMaterial)
                TSA_FIELD(rotation, setRotation)
                TSA_FIELD(color, setColor)
            });
        break;
    case ElementKind::TrussMember:
        run([&](int id) { return model.getTrussMember(id); }, std::get<TrussMember>(m_before), diff.trussMembers,
            diff.modifiedTrussMemberIds, [](const TrussMember& b, const TrussMember& a, TrussMember& d, bool& c) {
                TSA_FIELD(role, setRole)
                TSA_FIELD(section, setSection)
                TSA_FIELD(material, setMaterial)
                TSA_FIELD(color, setColor)
            });
        break;
    case ElementKind::Slab:
        run([&](int id) { return model.getSlab(id); }, std::get<Slab>(m_before), diff.slabs, diff.modifiedSlabIds,
            [](const Slab& b, const Slab& a, Slab& d, bool& c) {
                TSA_FIELD(thickness, setThickness)
                TSA_FIELD(material, setMaterial)
                TSA_FIELD(slabType, setSlabType)
                TSA_FIELD(color, setColor)
            });
        break;
    case ElementKind::Wall:
        run([&](int id) { return model.getWall(id); }, std::get<Wall>(m_before), diff.walls, diff.modifiedWallIds,
            [](const Wall& b, const Wall& a, Wall& d, bool& c) {
                TSA_FIELD(height, setHeight)
                TSA_FIELD(thickness, setThickness)
                TSA_FIELD(material, setMaterial)
                TSA_FIELD(offset, setOffset)
                TSA_FIELD(color, setColor)
            });
        break;
    case ElementKind::Foundation:
        run([&](int id) { return model.getFoundation(id); }, std::get<Foundation>(m_before), diff.foundations,
            diff.modifiedFoundationIds, [](const Foundation& b, const Foundation& a, Foundation& d, bool& c) {
                TSA_FIELD(foundationType, setFoundationType)
                TSA_FIELD(widthA, setWidthA)
                TSA_FIELD(lengthB, setLengthB)
                TSA_FIELD(heightH, setHeightH)
                TSA_FIELD(material, setMaterial)
                TSA_FIELD(soilBearingCapacity, setSoilBearingCapacity)
                TSA_FIELD(color, setColor)
            });
        break;
    default:
        break;
    }
    if (!diff.isEmpty()) model.setModified(true);
    return diff;
}

#undef TSA_FIELD

} // namespace TSA::Model
