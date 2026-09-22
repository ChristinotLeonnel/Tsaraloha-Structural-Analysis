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
            emit activeGridChanged(m_activeGridId);
        }
        emit gridRemoved(id);
        return true;
    }
    return false;
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

void GridManager::deserializeFromJson(const std::string& /*json*/)
{
    // Extensible pour futurs fichiers projets
}

} // namespace TSA::Grid
