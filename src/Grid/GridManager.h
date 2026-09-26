#pragma once

#include <QObject>
#include "GridSystem.h"
#include <vector>
#include <memory>
#include <string>

namespace TSA::Grid
{

class GridManager : public QObject
{
    Q_OBJECT

public:
    explicit GridManager(QObject* parent = nullptr);
    ~GridManager() override = default;

    // Gestion de la collection de grilles
    const std::vector<std::unique_ptr<GridSystem>>& grids() const { return m_grids; }

    GridSystem* addGrid(const GridDefinition& definition);
    bool updateGrid(const std::string& id, const GridDefinition& definition);
    bool removeGrid(const std::string& id);
    GridSystem* duplicateGrid(const std::string& id);
    void clearAllGrids();

    GridSystem* getGrid(const std::string& id);
    const GridSystem* getGrid(const std::string& id) const;

    // Grille active utilisée pour le Snapping et le dessin
    GridSystem* activeGrid();
    const GridSystem* activeGrid() const;
    void setActiveGridId(const std::string& id);
    const std::string& activeGridId() const { return m_activeGridId; }

    // Raccourcis de visibilité
    void setGridVisible(const std::string& id, bool visible);
    void setAllGridsVisible(bool visible);

    // Sérialisation
    std::string serializeToJson() const;
    void deserializeFromJson(const std::string& json);

signals:
    void gridAdded(const std::string& id);
    void gridModified(const std::string& id);
    void gridRemoved(const std::string& id);
    void activeGridChanged(const std::string& id);
    void gridVisibilityChanged(const std::string& id, bool visible);

private:
    std::vector<std::unique_ptr<GridSystem>> m_grids;
    std::string m_activeGridId;
};

} // namespace TSA::Grid
