#pragma once

// Templates de projets (principe des gabarits AutoCAD) : un projet enregistré comme modèle réutilisable.
//
// Un template est un fichier projet .tsa ordinaire (format et compatibilité inchangés) rangé dans
// AppPaths::userProjectTemplatesDir(), accompagné d'une fiche « <nom>.json » :
//   { "format": "tsa-project-template/1", "name": "…", "description": "…", "created": "ISO 8601" }
// Il reprend tout le contenu du projet : grille et axes (coordonnées réelles), niveaux, plans de travail, objets,
// cas de charge, réglages d'analyse et de maillage, vues nommées. Un .tsa n'enregistre aucun résultat de calcul.
// Un projet créé depuis un template est une copie indépendante : le fichier du template n'est que lu.

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace TSA::Model
{
class Model;
}
namespace TSA::Grid
{
class GridManager;
}

namespace TSA::Project
{

struct ProjectTemplateInfo
{
    QString name;
    QString description;
    QDateTime created;
    QString path;       ///< fichier .tsa du template
};

class ProjectTemplateStore
{
    Q_DECLARE_TR_FUNCTIONS(TSA::Project::ProjectTemplateStore)

public:
    /// Dossier des templates (défaut : AppPaths::userProjectTemplatesDir()).
    explicit ProjectTemplateStore(const QString& directory = QString());

    const QString& directory() const { return m_directory; }

    /// Templates disponibles, triés par nom. Une fiche absente ou illisible n'écarte pas le template (nom = nom du
    /// fichier) ; elle est signalée dans *problems.
    std::vector<ProjectTemplateInfo> list(QStringList* problems = nullptr) const;
    /// Template de ce nom (comparaison sans casse ni espaces de bord), s'il existe.
    std::optional<ProjectTemplateInfo> find(const QString& name) const;

    enum class Overwrite
    {
        Refuse,  ///< nom déjà pris : refus avec message (jamais d'écrasement silencieux)
        Replace  ///< remplacement confirmé par l'utilisateur
    };
    /// Enregistre le projet comme template. Le modèle et la grille ne sont que lus : le projet source, son chemin et
    /// son état « modifié » restent inchangés.
    bool save(const QString& name, const QString& description, const TSA::Model::Model& model, const TSA::Grid::GridManager* grids,
              Overwrite overwrite, QString* error = nullptr, ProjectTemplateInfo* saved = nullptr) const;
    /// Charge un template dans le modèle et la grille. Le fichier est d'abord lu et vérifié dans un modèle à part :
    /// en cas d'erreur, le modèle cible n'est pas modifié. Repère et niveaux reconstruits (Grid/GridCoordinateSync).
    bool load(const ProjectTemplateInfo& info, TSA::Model::Model& model, TSA::Grid::GridManager* grids, QString* error = nullptr) const;

    /// Crée le fichier d'un nouveau projet, copie indépendante du template (nom du projet = projectName). Refuse
    /// d'écraser un fichier existant ; le template n'est que lu.
    bool createProject(const ProjectTemplateInfo& info, const QString& projectPath, const QString& projectName, QString* error = nullptr) const;

    /// Nom de fichier sûr dérivé du nom affiché (caractères interdits remplacés) ; vide si le nom est inutilisable.
    static QString fileBaseName(const QString& name);

private:
    QString m_directory;
};

} // namespace TSA::Project
