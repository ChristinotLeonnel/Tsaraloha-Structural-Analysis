#pragma once

#include <QObject>
#include <map>
#include <vector>
#include <memory>
#include <string>
#include "WorkPlane.h"

namespace TSA::Coordinate
{

/**
 * @brief Gestionnaire centralisé de multiples plans de travail 3D (Règle 14).
 * Permet de créer, modifier, supprimer, renommer, sélectionner et sérialiser
 * l'ensemble des plans de travail d'un projet TSA.
 */
class WorkPlaneManager : public QObject
{
    Q_OBJECT

public:
    explicit WorkPlaneManager(QObject* parent = nullptr);
    ~WorkPlaneManager() override = default;

    // Gestion de la collection
    int addWorkPlane(const WorkPlane& wp);
    bool removeWorkPlane(int id);
    WorkPlane* getWorkPlane(int id);
    const WorkPlane* getWorkPlane(int id) const;

    void updateWorkPlane(const WorkPlane& wp);

    const std::map<int, WorkPlane>& workPlanes() const noexcept { return m_workPlanes; }
    size_t count() const noexcept { return m_workPlanes.size(); }

    // Plan de travail actif
    int activeWorkPlaneId() const noexcept { return m_activeWorkPlaneId; }
    bool setActiveWorkPlane(int id);
    WorkPlane* activeWorkPlane();
    const WorkPlane* activeWorkPlane() const;

    // Réinitialisation avec plans par défaut (XY, XZ, YZ)
    void resetToDefault();
    void clear();

    // Sérialisation JSON
    std::string serializeToJson() const;
    bool deserializeFromJson(const std::string& jsonStr);

signals:
    void workPlaneAdded(int id);
    void workPlaneModified(int id);
    void workPlaneRemoved(int id);
    void activeWorkPlaneChanged(int id);

private:
    std::map<int, WorkPlane> m_workPlanes;
    int m_activeWorkPlaneId = 1;
    int m_nextId = 1;
};

} // namespace TSA::Coordinate
