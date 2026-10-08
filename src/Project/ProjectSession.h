#pragma once

// Session de projet partagée par les applications de l'écosystème (TSA, TSALab) — ADR-024.
//
// Regroupe ce qui constitue UN projet ouvert, indépendamment de toute fenêtre : le modèle structural
// (source de vérité, avec son UndoManager), le gestionnaire de commandes, les grilles et leur
// accrochage, le gestionnaire de fichier projet. Une fenêtre (MainWindow de TSA, fenêtre de TSALab)
// possède une session et y branche ses vues (viewport partagé, arbres, panneaux) ; elle ne crée plus
// ces objets elle-même.
//
// Couche modèle (aucun widget). La sélection est un état de vue : elle reste dans la couche
// graphique (TSA::Viewer::SelectionManager) et appartient à la fenêtre.

#include <memory>

class QObject;

namespace TSA::Model
{
class Model;
}
namespace TSA::UndoRedo
{
class CommandManager;
}
namespace TSA::Grid
{
class GridManager;
class GridSnapManager;
}

namespace TSA::Project
{

class ProjectManager;

class ProjectSession
{
public:
    /// @param owner parent Qt du gestionnaire de projet (signaux, boîtes de dialogue éventuelles).
    explicit ProjectSession(QObject* owner = nullptr);
    ~ProjectSession();

    ProjectSession(const ProjectSession&) = delete;
    ProjectSession& operator=(const ProjectSession&) = delete;

    TSA::Model::Model& model() { return *m_model; }
    const TSA::Model::Model& model() const { return *m_model; }
    TSA::UndoRedo::CommandManager& commands() { return *m_commands; }
    TSA::Grid::GridManager& grids() { return *m_grids; }
    const TSA::Grid::GridManager& grids() const { return *m_grids; }
    TSA::Grid::GridSnapManager& gridSnap() { return *m_gridSnap; }
    ProjectManager& project() { return *m_project; }
    const ProjectManager& project() const { return *m_project; }

private:
    // Ordre de destruction inverse : projet, accrochage, grilles, commandes, puis le modèle.
    std::unique_ptr<TSA::Model::Model> m_model;
    std::unique_ptr<TSA::UndoRedo::CommandManager> m_commands;
    std::unique_ptr<TSA::Grid::GridManager> m_grids;
    std::unique_ptr<TSA::Grid::GridSnapManager> m_gridSnap;
    std::unique_ptr<ProjectManager> m_project;
};

} // namespace TSA::Project
