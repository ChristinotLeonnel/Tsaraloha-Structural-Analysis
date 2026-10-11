#pragma once

// Visualisation d'un maillage TSA (docs/MESH_VISUALIZATION.md) dans la vue 3D OCCT existante (OccView) :
// - MeshVS (OCCT) : faces des éléments 2D, faces affichées des volumes, liens 1D, nœuds, étiquettes,
//   sélection de nœuds et d'éléments (identifiants métier) ;
// - un tableau de segments : arêtes UNIQUES (une arête partagée n'est tracée qu'une fois) ;
// - calques au premier plan : éléments de qualité insuffisante, entité mise en évidence ;
// - champs de résultats (MeshResultField) : couleurs nodales + légende, forme déformée, séparés du maillage.
// Purement visuel : ne modifie ni le modèle ni le maillage (le maillage affiché est une copie).

#include "../Mesh/MeshDisplayModel.h"
#include "../Mesh/MeshResultField.h"

#include <AIS_InteractiveContext.hxx>
#include <Bnd_Box.hxx>
#include <AIS_InteractiveObject.hxx>
#include <SelectMgr_EntityOwner.hxx>
#include <V3d_View.hxx>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class MeshVS_Mesh;
class AIS_ColorScale;

namespace TSA::Viewer
{

class MeshVSDataSource;

struct MeshViewOptions
{
    enum class Style
    {
        Wireframe,      ///< arêtes uniques seules
        FacesEdges,     ///< faces + arêtes uniques
        Faces,          ///< faces seules
        Shrink          ///< éléments réduits (inspection de la connectivité)
    };
    Style style = Style::FacesEdges;
    double transparency = 0.0;          ///< faces : 0 opaque … 1 invisible
    bool showNodes = false;
    double nodeSize = 2.0;              ///< échelle des marqueurs de nœuds
    double lineWidth = 1.5;             ///< arêtes et éléments 1D (pixels)
    bool nodeLabels = false;
    bool elementLabels = false;
    std::size_t maxLabels = 1000;       ///< au-delà : sous-ensemble réparti (lisibilité, performances)
    bool showUndeformed = true;         ///< forme initiale (arêtes grises) sous la forme déformée
    TSA::Mesh::MeshDisplayFilter filter;
};

class MeshVisual
{
public:
    enum class Mode
    {
        Generated,
        Preview
    };

    /// Entité sélectionnée (identifiant métier) et sa description.
    struct Pick
    {
        bool element = false;
        int id = 0;
        std::string text;
    };

    MeshVisual(const Handle(AIS_InteractiveContext)& context, const Handle(V3d_View)& view);
    /// Contexte et vue lus à chaque usage : la vue OCCT d'OccView n'existe qu'après son premier affichage.
    MeshVisual(std::function<Handle(AIS_InteractiveContext)()> context, std::function<Handle(V3d_View)()> view);
    ~MeshVisual();

    /// Affiche une copie du maillage. @param quality qualité par élément (index dans FeMesh::elements) ;
    /// éléments < threshold signalés en rouge. Les résultats précédents sont retirés ; ceux fournis ici
    /// (calculés pour CE maillage) sont appliqués dans la même construction (voir setResults).
    void show(const TSA::Mesh::FeMesh& mesh, Mode mode, const std::vector<double>* quality = nullptr, double threshold = 0.0,
              const TSA::Mesh::MeshResultField* colorField = nullptr, const TSA::Mesh::MeshResultField* displacement = nullptr,
              double scale = 0.0);
    void clear();
    bool visible() const { return !m_meshVS.IsNull() || !m_edges.IsNull(); }

    void setOptions(const MeshViewOptions& options);
    const MeshViewOptions& options() const { return m_options; }

    /// Champs de résultats : couleur (scalaire ou norme) et/ou déplacements (forme déformée × scale).
    /// Les nœuds sans valeur restent gris (jamais de valeur inventée). nullptr : champ retiré.
    void setResults(const TSA::Mesh::MeshResultField* colorField, const TSA::Mesh::MeshResultField* displacement, double scale);
    void setDeformationScale(double scale);
    double deformationScale() const { return m_scale; }
    bool hasResults() const { return m_colorField.has_value() || m_displacement.has_value(); }

    /// Mode sélection : nœuds et éléments du maillage sélectionnables au clic (sinon : aucune interférence avec
    /// la sélection du modèle).
    void setPickMode(bool on);
    bool pickMode() const { return m_pickMode; }
    /// Clic sur une entité détectée : vrai si elle appartient à ce maillage (mise en évidence, description).
    bool handlePick(const Handle(SelectMgr_EntityOwner)& owner, Pick* out);
    /// Texte de survol (vide si l'entité n'appartient pas à ce maillage).
    std::string hoverText(const Handle(SelectMgr_EntityOwner)& owner) const;

    std::string describeElement(int elementId) const;
    std::string describeNode(int nodeId) const;

    /// Met en évidence un élément (ou un nœud si elementId = 0) et cadre éventuellement la vue autour.
    void highlight(int elementId, int nodeId = 0, bool zoom = true);
    void clearHighlight();
    /// Cadre la vue sur le maillage affiché.
    void fit();
    /// Boîte englobante des nœuds affichés (positions déformées le cas échéant) ; vide si rien n'est affiché.
    Bnd_Box bounds() const;

    const TSA::Mesh::MeshDisplayModel& model() const { return m_model; }
    const TSA::Mesh::FeMesh* mesh() const { return m_mesh.get(); }
    Mode mode() const { return m_mode; }
    /// Durée de la dernière construction de l'affichage (ms) et nombre de segments d'arêtes.
    double lastBuildMs() const { return m_lastBuildMs; }
    std::size_t edgeSegmentCount() const { return m_edgeSegments; }
    std::size_t labelCount() const { return m_labelCount; }
    bool labelsTruncated() const { return m_labelsTruncated; }
    const Handle(MeshVS_Mesh)& meshObject() const { return m_meshVS; }

private:
    void rebuild();
    void removeObjects();
    void displayLegend();
    void activatePick();
    gp_Pnt position(int nodeId) const;

    Handle(AIS_InteractiveContext) context() const { return m_contextFn ? m_contextFn() : Handle(AIS_InteractiveContext)(); }
    Handle(V3d_View) view() const { return m_viewFn ? m_viewFn() : Handle(V3d_View)(); }

    std::function<Handle(AIS_InteractiveContext)()> m_contextFn;
    std::function<Handle(V3d_View)()> m_viewFn;
    std::shared_ptr<const TSA::Mesh::FeMesh> m_mesh;
    Mode m_mode = Mode::Generated;
    std::vector<double> m_quality;
    double m_threshold = 0.0;
    MeshViewOptions m_options;
    TSA::Mesh::MeshDisplayModel m_model;
    Handle(MeshVSDataSource) m_source;
    Handle(MeshVS_Mesh) m_meshVS;
    Handle(AIS_InteractiveObject) m_edges, m_poor, m_results, m_highlight;
    Handle(AIS_ColorScale) m_legend;
    std::optional<TSA::Mesh::MeshResultField> m_colorField, m_displacement;
    double m_scale = 0.0;
    bool m_pickMode = false;
    int m_highlightElement = 0, m_highlightNode = 0;
    double m_lastBuildMs = 0.0;
    std::size_t m_edgeSegments = 0, m_labelCount = 0;
    bool m_labelsTruncated = false;
};

} // namespace TSA::Viewer
