#pragma once

// Menus d'affichage partagés par TSA et TSALab (couche widgets, docs/TSARALOHA_ARCHITECTURE.md) :
//   « Échelle des annotations », « Échelle des charges », « Profils d'affichage » et « Diagnostic d'affichage… ».
// Représentation seulement (docs/DISPLAY_SCALES.md) : aucune action ne modifie le modèle ni les résultats.
// La fenêtre hôte fournit la vue et le modèle du document par des fonctions (relues à chaque action : le document
// au premier plan peut changer) ; elle resynchronise ses propres boutons sur le signal profileApplied.

#include <QObject>
#include <QString>

#include <functional>

class OccView;
class QAction;
class QMenu;
class QWidget;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

class DisplayMenus : public QObject
{
    Q_OBJECT

public:
    struct Context
    {
        std::function<OccView*()> view;                         ///< vue du document (peut être nulle)
        std::function<const TSA::Model::Model*()> model;        ///< modèle du document (envergure, adaptation)
        std::function<QString()> documentPath;                  ///< fichier du document, vide s'il n'est pas enregistré
    };

    /// dialogParent : parent des boîtes de saisie et des menus (fenêtre hôte).
    DisplayMenus(QWidget* dialogParent, Context context);

    QMenu* annotationMenu() const { return m_annotationMenu; }
    QMenu* axisMenu() const { return m_axisMenu; }
    QMenu* loadScaleMenu() const { return m_loadMenu; }
    QMenu* profilesMenu() const { return m_profilesMenu; }
    QAction* diagnosticAction() const { return m_diagnostic; }
    /// Ajoute les trois menus et le diagnostic à un menu de la fenêtre hôte.
    void addTo(QMenu* menu);

    /// Applique un profil enregistré (réglages graphiques seulement) ; associé au document s'il a un fichier.
    /// Faux si le profil n'existe pas ou s'il n'y a pas de vue.
    bool applyProfile(const QString& name);
    QString activeProfile() const { return m_activeProfile; }
    /// Texte du diagnostic d'affichage de la vue courante (vide sans vue).
    QString diagnosticText() const;

signals:
    void profileApplied(const QString& name);
    void message(const QString& text, const QString& type);   ///< type : INFO, WARN

private:
    void buildAnnotationMenu();
    void buildAxisMenu();
    void buildLoadMenu();
    void rebuildProfilesMenu();
    OccView* view() const { return m_context.view ? m_context.view() : nullptr; }

    QWidget* m_parent = nullptr;
    Context m_context;
    QMenu* m_annotationMenu = nullptr;
    QMenu* m_axisMenu = nullptr;
    QMenu* m_loadMenu = nullptr;
    QMenu* m_profilesMenu = nullptr;
    QAction* m_diagnostic = nullptr;
    QString m_activeProfile;
};

} // namespace TSA::UI
