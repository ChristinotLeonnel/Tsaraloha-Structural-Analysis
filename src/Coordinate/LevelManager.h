#pragma once

#include <QObject>
#include "Level.h"
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace TSA::Coordinate
{

class LevelManager : public QObject
{
    Q_OBJECT

public:
    explicit LevelManager(QObject* parent = nullptr);
    ~LevelManager() override = default;

    const std::vector<Level>& levels() const { return m_levels; }
    size_t levelCount() const { return m_levels.size(); }
    bool isEmpty() const { return m_levels.empty(); }

    Level* addLevel(const std::string& name, double elevation);
    Level* addLevelWithId(const std::string& id, const std::string& name, double elevation);
    bool removeLevel(const std::string& id);

    Level* getLevel(const std::string& id);
    const Level* getLevel(const std::string& id) const;

    Level* getLevelByIndex(size_t index);
    const Level* getLevelByIndex(size_t index) const;

    int getLevelIndex(const std::string& id) const;

    bool setLevelElevation(const std::string& id, double newElevation);
    bool setLevelName(const std::string& id, const std::string& newName);
    bool setLevelVisible(const std::string& id, bool visible);

    // Recherche géométrique
    const Level* findLevelAtElevation(double z, double tolerance = 1e-3) const;
    const Level* findClosestLevel(double z) const;

    std::vector<double> elevationList() const;
    std::vector<std::string> levelNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_levels.size());
        for (const auto& lvl : m_levels)
        {
            names.push_back(lvl.name);
        }
        return names;
    }

    // Définitions absolue vs relative
    void setFromElevations(const std::vector<double>& elevations, const std::vector<std::string>& names = {});
    void setFromSpacings(double baseElevation, const std::vector<double>& spacings, const std::vector<std::string>& names = {});
    std::vector<double> getSpacings() const;

    void clear();

    // Sérialisation JSON
    std::string serializeToJson() const;
    void deserializeFromJson(const std::string& json);

signals:
    void levelAdded(const std::string& levelId);
    void levelRemoved(const std::string& levelId);
    void levelModified(const std::string& levelId);
    void levelElevationChanged(const std::string& levelId, double oldElevation, double newElevation);
    void levelsChanged();

private:
    void sortLevels();
    std::string generateDefaultName(size_t index, double elevation) const;

private:
    std::vector<Level> m_levels;
    int m_nextLevelCounter = 0;
};

} // namespace TSA::Coordinate
