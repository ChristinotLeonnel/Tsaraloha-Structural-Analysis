#pragma once

#include <QObject>
#include <V3d_View.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include "../Coordinate/WorkPlane.h"

namespace TSA::Viewer
{

/**
 * @brief Énumération des vues de caméra standard dans TSA (Section 5, 7, 8).
 */
enum class StandardCameraView
{
    ThreeD_Axo,     // Vue Axonométrique 3D
    Top,            // Vue de Dessus (regard vers -Z)
    Bottom,         // Vue de Dessous (regard vers +Z)
    Front,          // Vue de Face (regard vers +Y)
    Back,           // Vue Arrière (regard vers -Y)
    Left,           // Vue Gauche (regard vers +X)
    Right,          // Vue Droite (regard vers -X)
    NormalToPlane,  // Vue perpendiculaire face au WorkPlane actif (+Zwp)
    OppositePlane,  // Vue arrière du WorkPlane actif (-Zwp)
    Custom          // Orientation libre
};

/**
 * @brief Orientation d'une caméra : direction de visée (de l'œil vers la cible) et haut de l'écran.
 * Base orthonormée garantie par les fonctions qui la produisent (droite de l'écran = direction ^ haut).
 */
struct CameraFrame
{
    gp_Dir direction;
    gp_Dir up;
    gp_Dir right() const { return direction.Crossed(up); }
};

/**
 * @brief Gestionnaire de caméra et de vue spécialisé (Règle 5, 7 & 11).
 * Contrôle exclusivement :
 * - Orientation de la caméra
 * - Cadrage (FitAll, Zoom)
 * - Type de projection caméra (Perspective vs Orthographique)
 * - Vues Face, Arrière, Dessus, Dessous, Gauche, Droite, Normal au plan
 *
 * Seule source des orientations de TSA (vues standard, ViewCube, plan de travail) : convention Z vertical, vues
 * déterministes (indépendantes de la vue de départ), appliquées en une fois à la caméra.
 *
 * IMPORTANT : Ne modifie JAMAIS le modèle structural ni les coordonnées du WorkPlane.
 */
class ViewManager : public QObject
{
    Q_OBJECT

public:
    explicit ViewManager(QObject* parent = nullptr);
    ~ViewManager() override = default;

    StandardCameraView currentView() const noexcept { return m_currentView; }
    void setCurrentView(StandardCameraView v) noexcept { m_currentView = v; }

    bool isOrthographic() const noexcept { return m_isOrthographic; }
    void setOrthographic(bool ortho, const Handle(V3d_View)& view);
    /// Projection réellement appliquée à la caméra de la vue (et non un état mémorisé à part).
    static bool cameraIsOrthographic(const Handle(V3d_View)& view);

    /// Orientation d'une direction du cube (faces, arêtes, coins ; V3d_TypeOfOrientation, Z vertical) :
    /// regard vers la cible, haut = +Z rendu orthogonal ; dessus : haut +Y, dessous : haut -Y (X vers la droite).
    static CameraFrame frameForOrientation(V3d_TypeOfOrientation orientation);
    /// Vue standard (Top…Right, ThreeD_Axo) ; les vues relatives au plan passent par planeFrame().
    static CameraFrame standardFrame(StandardCameraView view);
    static V3d_TypeOfOrientation orientationOf(StandardCameraView view);
    /// Face au plan : regard selon -N (œil du côté +N), haut = axe V du plan ; opposite : de l'autre côté.
    static CameraFrame planeFrame(const TSA::Coordinate::WorkPlane& wp, bool opposite = false);
    /// Applique l'orientation en une fois (œil, cible, haut), sans dépendre de l'orientation précédente.
    static void applyFrame(const Handle(V3d_View)& view, const CameraFrame& frame, const gp_Pnt& center);

    // Application d'une orientation standard
    void applyStandardView(StandardCameraView stdView,
                           const Handle(V3d_View)& view,
                           const TSA::Coordinate::WorkPlane& wp);

    // Orientation face au plan actif
    void viewNormalToWorkPlane(const Handle(V3d_View)& view,
                               const TSA::Coordinate::WorkPlane& wp,
                               bool opposite = false);

signals:
    void cameraViewChanged(StandardCameraView view);
    void projectionTypeChanged(bool isOrthographic);

private:
    StandardCameraView m_currentView = StandardCameraView::ThreeD_Axo;
    bool m_isOrthographic = true;   // caméra OCCT orthographique par défaut
};

} // namespace TSA::Viewer
