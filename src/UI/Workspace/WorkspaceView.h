#pragma once

// Espace de travail central d'un document : la vue du modèle (3D) et l'éditeur de Blueprint, côte à côte dans un
// séparateur, selon trois dispositions (docs/UI.md, « Espace de travail ») :
//   Modèle 3D  — la vue 3D seule ;
//   Blueprint  — le graphe seul, toute la surface centrale ;
//   Partagé    — les deux, redimensionnables, côte à côte ou l'une au-dessus de l'autre.
// Les deux vues représentent le MÊME projet : l'éditeur exécute ses Blueprints sur la session du document (registre
// central de commandes, historique Annuler / Rétablir du projet, observateurs du modèle) ; aucune copie du modèle.
// L'éditeur n'est créé qu'à la première demande (démarrage inchangé pour qui ne s'en sert pas).
// Préférences (disposition, tailles, orientation, panneaux de l'éditeur) : réglages de l'application, jamais le projet.

#include <QWidget>

#include <QList>

class QSettings;
class QSplitter;

namespace TSA::Project
{
class ProjectSession;
}

namespace TSA::UI
{

class BlueprintEditor;

class WorkspaceView : public QWidget
{
    Q_OBJECT

public:
    enum class Mode
    {
        Model3D,
        Blueprint,
        Split
    };

    /// @param modelView vue du modèle (conteneur du viewport OCCT), reparentée dans l'espace de travail.
    explicit WorkspaceView(QWidget* modelView, QWidget* parent = nullptr);

    void setSession(TSA::Project::ProjectSession* session);

    /// Éditeur de Blueprint, créé à la première demande (sur la session du document).
    BlueprintEditor* blueprint();
    bool hasBlueprint() const { return m_blueprint != nullptr; }
    QWidget* modelView() const { return m_modelView; }

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    Qt::Orientation splitOrientation() const;
    void setSplitOrientation(Qt::Orientation orientation);

    /// Disposition par défaut : Modèle 3D, partage horizontal à parts égales, panneaux de l'éditeur affichés.
    void resetLayout();
    /// Groupe « Workspace » des réglages de l'application.
    void saveState(QSettings& settings) const;
    /// Valeurs inconnues ou illisibles ignorées (disposition par défaut pour ce qu'elles décrivaient) ; jamais bloquant.
    void restoreState(QSettings& settings);

    static QString modeKey(Mode mode);
    /// Mode d'une clé de réglage ; faux si la clé est inconnue (*mode inchangé).
    static bool modeFromKey(const QString& key, Mode* mode);

signals:
    void modeChanged(TSA::UI::WorkspaceView::Mode mode);
    /// Éditeur tout juste créé : l'application y branche ses signaux (journal, projet modifié).
    void blueprintCreated(TSA::UI::BlueprintEditor* editor);

private:
    void applyMode();

    QWidget* m_modelView = nullptr;
    QSplitter* m_split = nullptr;
    BlueprintEditor* m_blueprint = nullptr;
    TSA::Project::ProjectSession* m_session = nullptr;
    Mode m_mode = Mode::Model3D;
    QList<int> m_splitSizes;                 ///< tailles du mode partagé (mémorisées entre deux passages)
    QByteArray m_pendingEditorState;         ///< disposition de l'éditeur à appliquer à sa création
};

} // namespace TSA::UI
