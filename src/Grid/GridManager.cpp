#include "GridManager.h"
#include <algorithm>
#include <sstream>

namespace TSA::Grid
{

GridManager::GridManager(QObject* parent)
    : QObject(parent)
{
    // Création de la grille principale par défaut
    GridDefinition defaultGrid("Main Grid", GridType::Cartesian);
    defaultGrid.generateCartesian(3, 5.0, 2, 4.0, 2, 3.0); // X: 0,5,10,15m ; Y: 0,4,8m ; Z: 0,3,6m
    addGrid(defaultGrid);
}

GridSystem* GridManager::addGrid(const GridDefinition& definition)
{
    auto system = std::make_unique<GridSystem>(definition);
    std::string id = system->id();

    // Une grille nouvellement ajoutée n'est jamais active par défaut,
    // sauf si c'est la toute première grille du projet.
    bool becomesActive = m_grids.empty();
    system->setActive(becomesActive);

    GridSystem* ptr = system.get();
    m_grids.push_back(std::move(system));

    if (becomesActive)
    {
        m_activeGridId = id;
    }

    emit gridAdded(id);
    return ptr;
}

bool GridManager::updateGrid(const std::string& id, const GridDefinition& definition)
{
    GridSystem* grid = getGrid(id);
    if (!grid)
        return false;

    grid->updateDefinition(definition);
    emit gridModified(id);
    return true;
}

bool GridManager::removeGrid(const std::string& id)
{
    auto it = std::find_if(m_grids.begin(), m_grids.end(),
        [&id](const std::unique_ptr<GridSystem>& g) { return g->id() == id; });

    if (it != m_grids.end())
    {
        m_grids.erase(it);
        if (m_activeGridId == id)
        {
            m_activeGridId = m_grids.empty() ? "" : m_grids.front()->id();

            // Correction : synchroniser le flag GridSystem::isActive() de chaque
            // grille restante avec le nouvel m_activeGridId. Sans cela, la grille
            // promue active reste marquée isActive() == false (valeur héritée
            // d'addGrid), et GridSnapManager::findSnap() / GridSystem::findClosestSnap()
            // l'ignorent silencieusement puisqu'ils se basent sur isActive().
            for (auto& g : m_grids)
            {
                g->setActive(g->id() == m_activeGridId);
            }

            emit activeGridChanged(m_activeGridId);
        }
        emit gridRemoved(id);
        return true;
    }
    return false;
}

GridSystem* GridManager::duplicateGrid(const std::string& id)
{
    GridSystem* source = getGrid(id);
    if (!source)
        return nullptr;

    GridDefinition dupDef = source->definition();
    static uint64_t dupCounter = 0;
    dupDef.setId("grid_" + std::to_string(++dupCounter) + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    dupDef.setName(source->definition().name() + " (Copie)");
    return addGrid(dupDef);
}

void GridManager::clearAllGrids()
{
    m_grids.clear();
    m_activeGridId.clear();
}

GridSystem* GridManager::getGrid(const std::string& id)
{
    for (auto& g : m_grids)
    {
        if (g->id() == id)
            return g.get();
    }
    return nullptr;
}

const GridSystem* GridManager::getGrid(const std::string& id) const
{
    for (const auto& g : m_grids)
    {
        if (g->id() == id)
            return g.get();
    }
    return nullptr;
}

GridSystem* GridManager::activeGrid()
{
    return getGrid(m_activeGridId);
}

const GridSystem* GridManager::activeGrid() const
{
    return getGrid(m_activeGridId);
}

void GridManager::setActiveGridId(const std::string& id)
{
    if (m_activeGridId != id && getGrid(id))
    {
        m_activeGridId = id;
        for (auto& g : m_grids)
        {
            g->setActive(g->id() == m_activeGridId);
        }
        emit activeGridChanged(m_activeGridId);
    }
}

void GridManager::setGridVisible(const std::string& id, bool visible)
{
    GridSystem* grid = getGrid(id);
    if (grid && grid->isVisible() != visible)
    {
        grid->setVisible(visible);
        emit gridVisibilityChanged(id, visible);
    }
}

void GridManager::setAllGridsVisible(bool visible)
{
    for (auto& g : m_grids)
    {
        g->setVisible(visible);
        emit gridVisibilityChanged(g->id(), visible);
    }
}

std::string GridManager::serializeToJson() const
{
    std::ostringstream oss;
    oss << "{\n  \"activeGridId\": \"" << m_activeGridId << "\",\n";
    oss << "  \"grids\": [\n";
    for (size_t i = 0; i < m_grids.size(); ++i)
    {
        oss << m_grids[i]->definition().toJson();
        if (i + 1 < m_grids.size()) oss << ",\n";
    }
    oss << "\n  ]\n}";
    return oss.str();
}

void GridManager::deserializeFromJson(const std::string& json)
{
    // Auparavant : fonction vide, aucune grille sauvegardée n'était jamais rechargée.
    auto findField = [&json](const std::string& field) -> std::string {
        std::string token = "\"" + field + "\"";
        size_t pos = json.find(token);
        if (pos == std::string::npos) return "";
        size_t colon = json.find(':', pos);
        if (colon == std::string::npos) return "";
        size_t start = colon + 1;
        while (start < json.size() && (json[start] == ' ' || json[start] == '\n' || json[start] == '\r'))
            start++;
        size_t end = json.find_first_of(",}\n\r", start);
        if (end == std::string::npos) end = json.size();
        std::string val = json.substr(start, end - start);
        if (!val.empty() && val.front() == '"') val = val.substr(1);
        if (!val.empty() && val.back() == '"') val.pop_back();
        return val;
    };

    const std::string activeId = findField("activeGridId");

    size_t arrPos = json.find("\"grids\"");
    if (arrPos == std::string::npos)
        return;
    size_t arrStart = json.find('[', arrPos);
    if (arrStart == std::string::npos)
        return;

    clearAllGrids();

    // Découpage des objets JSON de premier niveau du tableau "grids" en
    // comptant la profondeur des accolades (les tableaux internes comme
    // "xPositions": [...] ne contiennent pas d'accolades, donc pas de risque
    // de confusion).
    int depth = 0;
    size_t objStart = std::string::npos;
    for (size_t i = arrStart + 1; i < json.size(); ++i)
    {
        char c = json[i];
        if (c == '{')
        {
            if (depth == 0) objStart = i;
            ++depth;
        }
        else if (c == '}')
        {
            --depth;
            if (depth == 0 && objStart != std::string::npos)
            {
                std::string objStr = json.substr(objStart, i - objStart + 1);
                addGrid(GridDefinition::fromJson(objStr));
                objStart = std::string::npos;
            }
        }
        else if (c == ']' && depth == 0)
        {
            break; // fin du tableau "grids"
        }
    }

    if (!activeId.empty() && getGrid(activeId))
    {
        setActiveGridId(activeId);
    }
}

} // namespace TSA::Grid
