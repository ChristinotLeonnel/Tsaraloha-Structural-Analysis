#pragma once
// Copies de récupération des projets ouverts (BUG-157) : sauvegarde automatique périodique d'un document modifié, dans un
// dossier propre à l'application (jamais par-dessus le fichier du projet, sans le marquer « enregistré »), effacée à
// l'enregistrement et à la fermeture normale du document. Une copie restée après une fermeture brutale est proposée au
// démarrage suivant. Un verrou par document (QLockFile) empêche de proposer la copie d'un projet encore ouvert par une autre
// instance de l'application. Sans widget : partagé par TSA et TSALab (couche model).
#include <QDateTime>
#include <QString>

#include <memory>
#include <vector>

class QLockFile;

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

struct RecoveryEntry
{
    QString key;            ///< identifiant du document (une session de document)
    QString file;           ///< copie de récupération (.tsa)
    QString originalPath;   ///< fichier du projet ; vide : projet jamais enregistré
    QString title;          ///< titre affiché du projet
    QDateTime savedAt;      ///< date de la copie
};

class RecoveryStore
{
public:
    /// directory vide : <données locales de l'application>/recovery.
    explicit RecoveryStore(const QString& directory = QString());

    const QString& directory() const noexcept { return m_dir; }

    /// Verrou du document key, à garder tant que le document est ouvert (nul si le verrou n'a pas pu être pris).
    std::unique_ptr<QLockFile> lock(const QString& key) const;
    /// Écrit (remplace) la copie de récupération du document key et sa fiche.
    /// `title` : nom du projet (écrit aussi dans la copie, avec `author` : un projet récupéré les garde).
    /// `password` non vide : copie chiffrée comme le projet, fiche sans le nom (BUG-167 : rien en clair d'un projet protégé).
    bool write(const QString& key, const TSA::Model::Model& model, const TSA::Grid::GridManager* grids, const QString& originalPath,
               const QString& title, QString* error = nullptr, const QString& password = QString(), const QString& author = QString()) const;
    /// Efface la copie du document key et sa fiche (enregistrement, fermeture normale, copie récupérée ou abandonnée).
    void remove(const QString& key) const;
    /// Copies orphelines (document fermé brutalement : aucun processus ne tient leur verrou), les plus récentes d'abord.
    std::vector<RecoveryEntry> orphans() const;

    /// Intervalle de la sauvegarde automatique (minutes, réglage de l'application) ; 0 : désactivée. Défaut : 5.
    static int intervalMinutes();
    static void setIntervalMinutes(int minutes);

private:
    QString path(const QString& key, const char* suffix) const;
    QString m_dir;
};

} // namespace TSA::Project
